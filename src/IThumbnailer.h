/*****************************************************************************
 * IThumbnailer : IThumbnailer interface
 *
 *****************************************************************************
 * Copyright (C) 2016-2016 VideoLAN
 *
 * Authors: Paweł Wegner <pawel.wegner95@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef ITHUMBNAILER_H
#define ITHUMBNAILER_H

#include <memory>

#include "IItem.h"
#include "IRequest.h"

namespace cloudstorage {

class ICloudProvider;

class IThumbnailer {
 public:
  using Pointer = std::shared_ptr<IThumbnailer>;
  using Callback = std::function<void(EitherError<std::vector<char>>)>;

  virtual ~IThumbnailer() = default;

  /**
   * Should generate a thumbnail for an item.
   *
   * @param item item for which a thumbnail should be generated
   *
   * @param callback function to be called when the thumbnail generation is
   * finished
   *   *
   * @return object representing pending request
   */
  virtual IRequest<EitherError<std::vector<char>>>::Pointer generateThumbnail(
      std::shared_ptr<ICloudProvider>, IItem::Pointer item,
      Callback callback = [](EitherError<std::vector<char>>) {}) = 0;
};

}  // namespace cloudstorage

#endif  // ITHUMBNAILER_H
