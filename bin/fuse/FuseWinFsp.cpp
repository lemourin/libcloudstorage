#include "FuseWinFsp.h"

#ifdef WITH_WINFSP

#include <chrono>

#include "FuseCommon.h"
#include "IFileSystem.h"
#include "Utility/Utility.h"

const int ALLOCATION_UNIT = 4096;

namespace cloudstorage {

namespace {

struct FspContext {
  ~FspContext() {
    if (fs_) {
      FspFileSystemStopDispatcher(fs_);
      FspFileSystemDelete(fs_);
    }
  }

  IFileSystem *context() { return *context_; }

  FSP_FILE_SYSTEM *fs_ = nullptr;
  IFileSystem **context_;
};

struct FspFileContext {
  IFileSystem::INode::Pointer inode_;
  void *directory_buffer_ = nullptr;
};

void node_to_file_info(IFileSystem::INode *node,
                       FSP_FSCTL_FILE_INFO *file_info) {
  auto timestamp =
      10000000ull * (std::chrono::duration_cast<std::chrono::seconds>(
                         node->timestamp().time_since_epoch())
                         .count() +
                     11644473600LL);
  file_info->FileAttributes = node->type() == IItem::FileType::Directory
                                  ? FILE_ATTRIBUTE_DIRECTORY
                                  : FILE_ATTRIBUTE_NORMAL;
  file_info->ReparseTag = 0;
  file_info->FileSize =
      node->size() == IItem::UnknownSize ? UINT64_MAX : node->size();
  file_info->AllocationSize =
      (node->size() + ALLOCATION_UNIT - 1) / ALLOCATION_UNIT * ALLOCATION_UNIT;
  file_info->CreationTime = timestamp;
  file_info->LastAccessTime = timestamp;
  file_info->LastWriteTime = timestamp;
  file_info->ChangeTime = timestamp;
  file_info->IndexNumber = 0;
  file_info->HardLinks = 0;
}

std::string path(const wchar_t *str) {
  auto length = wcslen(str);
  std::wstring result;
  for (auto i = 0; i < length; i++) {
    if (str[i] == '\\')
      result += '/';
    else
      result += str[i];
  }
  return to_string(result);
}

std::string error_string(HRESULT r) {
  const int BUFFER_SIZE = 512;
  wchar_t buffer[BUFFER_SIZE] = {};
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, r, 0, buffer, BUFFER_SIZE,
                nullptr);
  return to_string(buffer);
}

NTSTATUS get_security_by_name(FSP_FILE_SYSTEM *fs, PWSTR filename,
                              PUINT32 attributes,
                              PSECURITY_DESCRIPTOR descriptor, SIZE_T *size) {
  *size = sizeof(SECURITY_DESCRIPTOR);
  return STATUS_SUCCESS;
}

NTSTATUS open(FSP_FILE_SYSTEM *fs, PWSTR filename, UINT32 create_options,
              UINT32 granted_access, PVOID *file_context,
              FSP_FSCTL_FILE_INFO *file_info) {
  auto c = static_cast<FspContext *>(fs->UserContext);
  std::promise<NTSTATUS> result;
  c->context()->getattr(path(filename), [&](EitherError<IFileSystem::INode> e) {
    if (e.left()) {
      *file_context = nullptr;
      return result.set_value(STATUS_OBJECT_NAME_INVALID);
    }
    auto node = e.right();
    node_to_file_info(node.get(), file_info);
    *file_context = new FspFileContext{node};
    result.set_value(STATUS_SUCCESS);
  });
  return result.get_future().get();
}

VOID close(FSP_FILE_SYSTEM *, PVOID file_context) {
  auto c = static_cast<FspFileContext *>(file_context);
  if (c->directory_buffer_) {
    FspFileSystemDeleteDirectoryBuffer(&c->directory_buffer_);
  }
  delete c;
}

NTSTATUS read_directory(FSP_FILE_SYSTEM *fs, PVOID file_context, PWSTR pattern,
                        PWSTR marker, PVOID buffer, ULONG buffer_length,
                        PULONG bytes_transferred) {
  auto c = static_cast<FspContext *>(fs->UserContext);
  auto file = static_cast<FspFileContext *>(file_context);
  auto hint = FspFileSystemGetOperationContext()->Request->Hint;
  auto transferred = *bytes_transferred;
  if (pattern) {
    NTSTATUS result;
    FspFileSystemReadDirectoryBuffer(&file->directory_buffer_, pattern, buffer,
                                     buffer_length, bytes_transferred);
    return STATUS_SUCCESS;
  }
  c->context()->readdir(
      file->inode_->inode(), [=](EitherError<IFileSystem::INode::List> e) {
        FSP_FSCTL_TRANSACT_RSP response;
        response.Size = sizeof(response);
        response.Kind = FspFsctlTransactQueryDirectoryKind;
        response.Hint = hint;
        if (e.left()) {
          response.IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
          response.IoStatus.Information = 0;
          FspFileSystemSendResponse(fs, &response);
          return;
        }
        auto list = *e.right();
        std::sort(list.begin(), list.end(),
                  [](const IFileSystem::INode::Pointer &n1,
                     const IFileSystem::INode::Pointer &n2) {
                    return n1->filename() < n2->filename();
                  });
        NTSTATUS result;
        if (!FspFileSystemAcquireDirectoryBuffer(&file->directory_buffer_, true,
                                                 &result)) {
          response.IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
          response.IoStatus.Information = 0;
          FspFileSystemSendResponse(fs, &response);
          return;
        }
        for (auto entry : list) {
          if (!marker ||
              c->context()->sanitize(entry->filename()) > to_string(marker)) {
            union {
              UINT8
              bytes[sizeof(FSP_FSCTL_DIR_INFO) + MAX_PATH * sizeof(WCHAR)];
              FSP_FSCTL_DIR_INFO d;
            } info;
            std::wstring filename =
                from_string(c->context()->sanitize(entry->filename()));
            info.d.Size = FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) +
                          filename.length() * sizeof(wchar_t);
            node_to_file_info(entry.get(), &info.d.FileInfo);
            memcpy(info.d.FileNameBuf, filename.c_str(),
                   filename.length() * sizeof(wchar_t));
            if (!FspFileSystemFillDirectoryBuffer(&file->directory_buffer_,
                                                  &info.d, &result)) {
              break;
            }
          }
        }
        FspFileSystemReleaseDirectoryBuffer(&file->directory_buffer_);
        ULONG bytes_transferred = transferred;
        FspFileSystemReadDirectoryBuffer(&file->directory_buffer_, marker,
                                         buffer, buffer_length,
                                         &bytes_transferred);
        response.IoStatus.Status = STATUS_SUCCESS;
        response.IoStatus.Information = bytes_transferred;
        FspFileSystemSendResponse(fs, &response);
      });
  return STATUS_PENDING;
}

NTSTATUS set_volume_label(FSP_FILE_SYSTEM *fs, PWSTR volume_label,
                          FSP_FSCTL_VOLUME_INFO *volume_info) {
  return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS get_volume_info(FSP_FILE_SYSTEM *,
                         FSP_FSCTL_VOLUME_INFO *volume_info) {
  volume_info->FreeSize = 0;
  volume_info->TotalSize = 0;
  wcscpy(volume_info->VolumeLabel, L"cloudstorage");
  volume_info->VolumeLabelLength = wcslen(volume_info->VolumeLabel);
  return S_OK;
}

NTSTATUS get_file_info(FSP_FILE_SYSTEM *fs, PVOID file_context,
                       FSP_FSCTL_FILE_INFO *file_info) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS get_security(FSP_FILE_SYSTEM *fs, PVOID file_context,
                      PSECURITY_DESCRIPTOR security_descriptor,
                      SIZE_T *security_descriptor_size) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS read(FSP_FILE_SYSTEM *fs, PVOID file_context, PVOID buffer,
              UINT64 offset, ULONG length, PULONG bytes_transferred) {
  auto c = static_cast<FspContext *>(fs->UserContext);
  auto file = static_cast<FspFileContext *>(file_context);
  auto hint = FspFileSystemGetOperationContext()->Request->Hint;
  if (offset >= file->inode_->size()) {
    return STATUS_END_OF_FILE;
  }
  c->context()->read(
      file->inode_->inode(), offset, length, [=](EitherError<std::string> e) {
        FSP_FSCTL_TRANSACT_RSP response;
        memset(&response, 0, sizeof(response));
        response.Size = sizeof(response);
        response.Kind = FspFsctlTransactReadKind;
        response.Hint = hint;
        if (e.left()) {
          response.IoStatus.Status = STATUS_INVALID_DEVICE_REQUEST;
          response.IoStatus.Information = 0;
          FspFileSystemSendResponse(fs, &response);
          return;
        }
        auto b = e.right();
        memcpy(buffer, b->c_str(), b->size());
        response.IoStatus.Status = STATUS_SUCCESS;
        response.IoStatus.Information = b->size();
        FspFileSystemSendResponse(fs, &response);
      });
  return STATUS_PENDING;
}

NTSTATUS write(FSP_FILE_SYSTEM *fs, PVOID file_context, PVOID buffer,
               UINT64 offset, ULONG length, BOOLEAN write_to_end_file,
               BOOLEAN constrained_io, PULONG bytes_transferred,
               FSP_FSCTL_FILE_INFO *file_info) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS set_basic_info(FSP_FILE_SYSTEM *fs, PVOID file_context,
                        UINT32 file_attributes, UINT64 creation_time,
                        UINT64 last_access_time, UINT64 last_write_time,
                        UINT64 change_time, FSP_FSCTL_FILE_INFO *file_info) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS set_file_size(FSP_FILE_SYSTEM *fs, PVOID file_context, UINT64 new_size,
                       BOOLEAN set_allocation_size,
                       FSP_FSCTL_FILE_INFO *file_info) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS set_security(FSP_FILE_SYSTEM *fs, PVOID file_context,
                      SECURITY_INFORMATION security_information,
                      PSECURITY_DESCRIPTOR modification_descriptor) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS flush(FSP_FILE_SYSTEM *fs, PVOID file_context,
               FSP_FSCTL_FILE_INFO *file_info) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS create(FSP_FILE_SYSTEM *fs, PWSTR filename, UINT32 create_options,
                UINT32 granted_access, UINT32 file_attributes,
                PSECURITY_DESCRIPTOR security_descriptor,
                UINT64 allocation_size, PVOID *file_context,
                FSP_FSCTL_FILE_INFO *file_info) {
  util::log("creating file", filename);
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS overwrite(FSP_FILE_SYSTEM *fs, PVOID file_context,
                   UINT32 file_attributes, BOOLEAN replace_file_attributes,
                   UINT64 allocation_size, FSP_FSCTL_FILE_INFO *file_info) {
  util::log("overwriting file");
  return STATUS_NOT_IMPLEMENTED;
}

VOID cleanup(FSP_FILE_SYSTEM *fs, PVOID file_context, PWSTR file_name,
             ULONG flags) {}

NTSTATUS can_delete(FSP_FILE_SYSTEM *fs, PVOID file_context, PWSTR file_name) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS rename(FSP_FILE_SYSTEM *fs, PVOID file_context, PWSTR filename,
                PWSTR new_filename, BOOLEAN replace_if_exists) {
  return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS fsp_start(FSP_SERVICE *service, ULONG argc, PWSTR *argv) {
  if (argc != 2) {
    return STATUS_INVALID_PARAMETER;
  }
  auto mountpoint = argv[1];

  static FSP_FSCTL_VOLUME_PARAMS volume_params = {};
  volume_params.SectorSize = ALLOCATION_UNIT;
  volume_params.SectorsPerAllocationUnit = 1;
  volume_params.VolumeSerialNumber = 1;
  volume_params.MaxComponentLength = MAX_PATH;
  volume_params.FileInfoTimeout = -1;
  volume_params.CaseSensitiveSearch = 1;
  volume_params.CasePreservedNames = 1;
  volume_params.UnicodeOnDisk = 1;
  volume_params.UmFileContextIsUserContext2 = 1;
  volume_params.ReadOnlyVolume = 1;
  wcscpy(volume_params.Prefix, L"\\cloud\\share");
  wcscpy(volume_params.FileSystemName, L"cloud");

  static FSP_FILE_SYSTEM_INTERFACE ops = winfsp_operations();
  std::unique_ptr<FspContext> context = util::make_unique<FspContext>();
  auto hr =
      FspFileSystemCreate(const_cast<PWSTR>(L"" FSP_FSCTL_NET_DEVICE_NAME),
                          &volume_params, &ops, &context->fs_);
  if (hr != S_OK) {
    util::log("failed to create file system", hr, error_string(hr),
              error_string(GetLastError()));
    return hr;
  }
  hr = FspFileSystemSetMountPoint(context->fs_, mountpoint);
  if (hr != S_OK) {
    util::log("failed to set mountpoint to", to_string(mountpoint));
    return hr;
  }
  hr = FspFileSystemStartDispatcher(context->fs_, 0);
  if (hr != S_OK) {
    util::log("failed to start dispatcher");
    return hr;
  }
  context->context_ = static_cast<IFileSystem **>(service->UserContext);
  context->fs_->UserContext = context.get();
  service->UserContext = context.release();
  return 0;
}

NTSTATUS fsp_stop(FSP_SERVICE *service) {
  delete static_cast<FspContext *>(service->UserContext);
  return 0;
}

}  // namespace

FuseWinFsp::FuseWinFsp(fuse_args *, const char *, void *userdata)
    : fs_(static_cast<IFileSystem **>(userdata)) {}

FuseWinFsp::~FuseWinFsp() {}

int FuseWinFsp::run(bool, bool) const {
  return FspServiceRunEx(const_cast<PWSTR>(L"cloudstorage-fuse"), fsp_start,
                         fsp_stop, nullptr, fs_);
}

FSP_FILE_SYSTEM_INTERFACE winfsp_operations() {
  FSP_FILE_SYSTEM_INTERFACE r = {};
  // r.GetSecurityByName = get_security_by_name;
  r.Open = open;
  r.Close = close;
  r.ReadDirectory = read_directory;
  r.GetVolumeInfo = get_volume_info;
  // r.GetFileInfo = get_file_info;
  // r.GetSecurity = get_security;
  r.Read = read;
  // r.SetVolumeLabelW = set_volume_label;
  // r.Write = write;
  // r.SetBasicInfo = set_basic_info;
  // r.SetFileSize = set_file_size;
  // r.SetSecurity = set_security;
  // r.Flush = flush;
  r.Create = create;
  r.Overwrite = overwrite;
  // r.Cleanup = cleanup;
  // r.CanDelete = can_delete;
  // r.Rename = rename;
  return r;
}

}  // namespace cloudstorage

#endif  // WITH_WINFSP
