#include "FuseDokan.h"

#ifdef WITH_DOKAN

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <algorithm>
#include <codecvt>
#include <cstdlib>
#include <cwchar>
#include <future>

#include "FuseCommon.h"
#include "IFileSystem.h"
#include "Utility/Utility.h"

namespace cloudstorage {

namespace {

struct FileId {
  std::string path_;
  std::string name_;
};

FileId file_id(const std::string &current_path) {
  auto slash_it = current_path.find_last_of("/");
  auto parent_path =
      std::string(current_path.begin(), current_path.begin() + slash_it + 1);
  auto name =
      std::string(current_path.begin() + slash_it + 1, current_path.end());
  return {parent_path, name};
}

IFileSystem **context_ptr;

IFileSystem *context() { return *context_ptr; }

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

void item_to_data(const IFileSystem::INode &item, WIN32_FIND_DATAW &data) {
  auto name = from_string(context()->sanitize(item.filename()));
  memcpy(data.cFileName, name.data(),
         sizeof(wchar_t) * std::min<size_t>(name.length(), MAX_PATH));
  data.dwFileAttributes |= item.type() == IItem::FileType::Directory
                               ? FILE_ATTRIBUTE_DIRECTORY
                               : FILE_ATTRIBUTE_NORMAL;
  if (item.size() != IItem::UnknownSize) {
    data.nFileSizeHigh = item.size() >> 32;
    data.nFileSizeLow = item.size() & ((1ull << 32) - 1);
  }
  if (item.timestamp() != IItem::UnknownTimeStamp) {
    auto timestamp =
        10000000ull * (std::chrono::duration_cast<std::chrono::seconds>(
                           item.timestamp().time_since_epoch())
                           .count() +
                       11644473600LL);
    data.ftCreationTime.dwHighDateTime = timestamp >> 32;
    data.ftCreationTime.dwLowDateTime = timestamp & ((1ull << 32) - 1);
  }
}

NTSTATUS
create_file(DOKAN_CREATE_FILE_EVENT *e) {
  auto current_path = path(e->FileName);
  auto parent_path = file_id(current_path).path_;
  auto name = file_id(current_path).name_;
  DWORD attributes;
  DWORD creation_disposition;
  DokanMapKernelToUserCreateFileFlags(e, &attributes, &creation_disposition);
  if (name != context()->sanitize(name)) return STATUS_INTERNAL_ERROR;
  if (creation_disposition == CREATE_ALWAYS ||
      creation_disposition == CREATE_NEW ||
      creation_disposition == OPEN_ALWAYS) {
    context()->getattr(current_path, [=](EitherError<IFileSystem::INode> d) {
      if (d.left()) {
        context()->getattr(parent_path, [=](EitherError<IFileSystem::INode> d) {
          if (d.left()) return DokanEndDispatchCreate(e, STATUS_INTERNAL_ERROR);
          if (e->DokanFileInfo->IsDirectory) {
            context()->mkdir(d.right()->inode(), name.c_str(),
                             [=](EitherError<IFileSystem::INode> d) {
                               if (d.left())
                                 return DokanEndDispatchCreate(
                                     e, STATUS_INTERNAL_ERROR);
                               DokanEndDispatchCreate(e, STATUS_SUCCESS);
                             });
          } else {
            auto node = context()->mknod(d.right()->inode(), name.c_str());
            context()->fsync(node, [=](EitherError<void> d) {
              if (d.left())
                return DokanEndDispatchCreate(e, STATUS_INTERNAL_ERROR);
              DokanEndDispatchCreate(e, STATUS_SUCCESS);
            });
          }
        });
      } else {
        if (creation_disposition == CREATE_NEW)
          DokanEndDispatchCreate(e, STATUS_OBJECT_NAME_COLLISION);
        else
          DokanEndDispatchCreate(e, STATUS_SUCCESS);
      }
    });
    return STATUS_PENDING;
  } else {
    context()->getattr(current_path, [=](EitherError<IFileSystem::INode> d) {
      if (d.left())
        DokanEndDispatchCreate(e, STATUS_NOT_FOUND);
      else {
        e->DokanFileInfo->IsDirectory =
            d.right()->type() == IItem::FileType::Directory;
        DokanEndDispatchCreate(e, STATUS_SUCCESS);
      }
    });
    return STATUS_PENDING;
  }
}

NTSTATUS find_files(DOKAN_FIND_FILES_EVENT *e) {
  util::log("find_file", path(e->PathName));
  context()->getattr(path(e->PathName), [=](EitherError<IFileSystem::INode> d) {
    if (d.left()) {
      return DokanEndDispatchFindFiles(e, STATUS_INTERNAL_ERROR);
    }
    context()->readdir(
        d.right()->inode(), [=](EitherError<IFileSystem::INode::List> d) {
          if (d.left()) {
            return DokanEndDispatchFindFiles(e, STATUS_INTERNAL_ERROR);
          }
          for (auto &&d : *d.right()) {
            WIN32_FIND_DATAW find_data = {};
            item_to_data(*d, find_data);
            e->FillFindData(e, &find_data);
          }
          DokanEndDispatchFindFiles(e, STATUS_SUCCESS);
        });
  });
  return STATUS_PENDING;
}

NTSTATUS get_volume_information(DOKAN_GET_VOLUME_INFO_EVENT *e) {
  const auto name = L"cloudstorage";
  memcpy(e->VolumeInfo->VolumeLabel, name, sizeof(wchar_t) * wcslen(name));
  e->VolumeInfo->VolumeLabelLength = static_cast<ULONG>(wcslen(name));
  return STATUS_SUCCESS;
}

NTSTATUS get_volume_attributes(DOKAN_GET_VOLUME_ATTRIBUTES_EVENT *e) {
  return STATUS_SUCCESS;
}

NTSTATUS read_file(DOKAN_READ_FILE_EVENT *e) {
  context()->getattr(path(e->FileName), [=](EitherError<IFileSystem::INode> d) {
    if (d.left()) return DokanEndDispatchRead(e, STATUS_INTERNAL_ERROR);
    if (e->Offset == d.right()->size()) {
      e->NumberOfBytesRead = 0;
      return DokanEndDispatchRead(e, STATUS_SUCCESS);
    }
    context()->read(
        d.right()->inode(), e->Offset, e->NumberOfBytesToRead,
        [=](EitherError<std::string> d) {
          if (d.left()) {
            return DokanEndDispatchRead(e, STATUS_INTERNAL_ERROR);
          }
          e->NumberOfBytesRead = static_cast<DWORD>(d.right()->length());
          memcpy(e->Buffer, d.right()->data(), e->NumberOfBytesRead);
          DokanEndDispatchRead(e, STATUS_SUCCESS);
        });
  });
  return STATUS_PENDING;
}

NTSTATUS file_information(DOKAN_GET_FILE_INFO_EVENT *e) {
  context()->getattr(path(e->FileName), [=](EitherError<IFileSystem::INode> d) {
    if (d.left()) {
      return DokanEndDispatchGetFileInformation(e, STATUS_INTERNAL_ERROR);
    }
    const auto &item = *d.right();
    e->FileHandleInfo.dwFileAttributes |=
        item.type() == IItem::FileType::Directory ? FILE_ATTRIBUTE_DIRECTORY
                                                  : FILE_ATTRIBUTE_NORMAL;
    if (item.size() != IItem::UnknownSize) {
      e->FileHandleInfo.nFileSizeHigh = item.size() >> 32;
      e->FileHandleInfo.nFileSizeLow = item.size() & ((1ull << 32) - 1);
    }
    if (item.timestamp() != IItem::UnknownTimeStamp) {
      auto timestamp =
          10000000ull * (std::chrono::duration_cast<std::chrono::seconds>(
                             item.timestamp().time_since_epoch())
                             .count() +
                         11644473600LL);
      e->FileHandleInfo.ftCreationTime.dwHighDateTime = timestamp >> 32;
      e->FileHandleInfo.ftCreationTime.dwLowDateTime =
          timestamp & ((1ull << 32) - 1);
    }
    DokanEndDispatchGetFileInformation(e, STATUS_SUCCESS);
  });
  return STATUS_PENDING;
}

NTSTATUS move_file(DOKAN_MOVE_FILE_EVENT *e) {
  auto commit_move = [=] {
    context()->getattr(
        file_id(path(e->FileName)).path_,
        [=](EitherError<IFileSystem::INode> d1) {
          if (d1.left())
            return DokanEndDispatchMoveFile(e, STATUS_INTERNAL_ERROR);
          auto name = file_id(path(e->FileName)).name_;
          auto new_name = file_id(path(e->NewFileName)).name_;
          context()->getattr(
              file_id(path(e->NewFileName)).path_,
              [=](EitherError<IFileSystem::INode> d2) {
                if (d2.left())
                  return DokanEndDispatchMoveFile(e, STATUS_INTERNAL_ERROR);
                context()->rename(d1.right()->inode(), name.c_str(),
                                  d2.right()->inode(), new_name.c_str(),
                                  [=](EitherError<IItem> r) {
                                    if (r.left())
                                      return DokanEndDispatchMoveFile(
                                          e, STATUS_INTERNAL_ERROR);
                                    DokanEndDispatchMoveFile(e, STATUS_SUCCESS);
                                  });
              });
        });
  };
  if (e->ReplaceIfExists == TRUE)
    commit_move();
  else {
    context()->getattr(
        path(e->NewFileName), [=](EitherError<IFileSystem::INode> d) {
          if (d.right())
            DokanEndDispatchMoveFile(e, STATUS_OBJECT_NAME_COLLISION);
          else
            commit_move();
        });
  }
  return STATUS_PENDING;
}

void cleanup(DOKAN_CLEANUP_EVENT *e) {
  auto current_path = path(e->FileName);
  auto parent_path = file_id(current_path).path_;
  auto name = file_id(current_path).name_;
  if (e->DokanFileInfo->DeleteOnClose) {
    context()->getattr(parent_path, [=](EitherError<IFileSystem::INode> d) {
      if (d.right()) {
        context()->remove(d.right()->inode(), name.c_str(),
                          [](EitherError<void>) {});
      }
    });
  } else {
    context()->getattr(current_path, [=](EitherError<IFileSystem::INode> d) {
      if (d.right() && d.right()->type() != IItem::FileType::Directory) {
        util::log("fsync cleanup", current_path);
        context()->fsync(d.right()->inode(), [](EitherError<void>) {});
      }
    });
  }
}

NTSTATUS can_delete_file(DOKAN_CAN_DELETE_FILE_EVENT *) {
  return STATUS_SUCCESS;
}

NTSTATUS write_file(DOKAN_WRITE_FILE_EVENT *e) {
  context()->getattr(path(e->FileName), [=](EitherError<IFileSystem::INode> d) {
    if (d.left()) return DokanEndDispatchWrite(e, STATUS_INTERNAL_ERROR);
    context()->write(
        d.right()->inode(), reinterpret_cast<const char *>(e->Buffer),
        e->NumberOfBytesToWrite, e->Offset, [=](EitherError<uint32_t> d) {
          if (d.left()) return DokanEndDispatchWrite(e, STATUS_INTERNAL_ERROR);
          e->NumberOfBytesWritten = *d.right();
          DokanEndDispatchWrite(e, STATUS_SUCCESS);
        });
  });
  return STATUS_PENDING;
}

NTSTATUS flush(DOKAN_FLUSH_BUFFERS_EVENT *e) {
  util::log("flush", path(e->FileName));
  context()->getattr(path(e->FileName), [=](EitherError<IFileSystem::INode> d) {
    if (d.left()) return DokanEndDispatchFlush(e, STATUS_INTERNAL_ERROR);
    context()->fsync(d.right()->inode(), [=](EitherError<void> d) {
      if (d.left()) return DokanEndDispatchFlush(e, STATUS_INTERNAL_ERROR);
      DokanEndDispatchFlush(e, STATUS_SUCCESS);
    });
  });
  return STATUS_PENDING;
}

NTSTATUS set_file_information(DOKAN_SET_FILE_BASIC_INFO_EVENT *e) {
  return STATUS_SUCCESS;
}

NTSTATUS volume_free_space(DOKAN_GET_DISK_FREE_SPACE_EVENT *e) {
  e->TotalNumberOfFreeBytes = 1024 * 1024 * 1024;
  e->FreeBytesAvailable = e->TotalNumberOfFreeBytes;
  e->TotalNumberOfBytes = e->TotalNumberOfFreeBytes;
  return STATUS_SUCCESS;
}

}  // namespace

DOKAN_OPERATIONS dokan_operations() {
  DOKAN_OPERATIONS result = {};
  result.ZwCreateFile = create_file;
  result.FindFiles = find_files;
  result.ReadFile = read_file;
  result.WriteFile = write_file;
  result.FlushFileBuffers = flush;
  result.Cleanup = cleanup;
  result.CanDeleteFile = can_delete_file;
  result.GetFileInformation = file_information;
  result.SetFileBasicInformation = set_file_information;
  result.GetVolumeInformationW = get_volume_information;
  result.GetVolumeAttributes = get_volume_attributes;
  result.MoveFileW = move_file;
  result.GetVolumeFreeSpace = volume_free_space;
  return result;
}

FuseDokan::FuseDokan(fuse_args *args, const char *mountpoint, void *userdata)
    : mountpoint_(from_string(mountpoint)), options_() {
  DokanInit(nullptr);
  options_.MountPoint = mountpoint_.c_str();
  options_.Version = DOKAN_VERSION;
  options_.Options = DOKAN_OPTION_NETWORK;
  context_ptr = static_cast<IFileSystem **>(userdata);
}

FuseDokan::~FuseDokan() { DokanShutdown(); }

int FuseDokan::run(bool singlethread, bool) const {
  DOKAN_OPERATIONS ops = dokan_operations();
  return DokanMain(const_cast<DOKAN_OPTIONS *>(&options_), &ops);
}

}  // namespace cloudstorage

#endif  // WITH_DOKAN