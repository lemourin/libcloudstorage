/*****************************************************************************
 * RecursiveRequest.cpp
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
#include "RecursiveRequest.h"

#include "CloudProvider/CloudProvider.h"

namespace cloudstorage {

template <class T>
RecursiveRequest<T>::RecursiveRequest(std::shared_ptr<CloudProvider> p,
                                      IItem::Pointer item,
                                      CompleteCallback callback,
                                      Visitor visitor)
    : Request<T>(p, callback, [=](typename Request<T>::Pointer r) {
        visit(
            r, item, [=](const T& e) { r->done(e); }, visitor);
      }) {}

template <class T>
void RecursiveRequest<T>::visit(typename Request<T>::Pointer r,
                                IItem::Pointer item, CompleteCallback callback,
                                Visitor visitor) {
  if (item->type() != IItem::FileType::Directory)
    visitor(r, item, callback);
  else
    r->make_subrequest(&CloudProvider::listDirectorySimpleAsync, item,
                       [=](EitherError<IItem::List> lst) {
                         if (lst.left()) return callback(lst.left());
                         visit(
                             r, lst.right(),
                             [=](const T& e) {
                               if (e.left()) return callback(e);
                               visitor(r, item, callback);
                             },
                             visitor);
                       });
}

template <class T>
void RecursiveRequest<T>::visit(typename Request<T>::Pointer r,
                                std::shared_ptr<IItem::List> lst,
                                CompleteCallback callback, Visitor visitor) {
  if (lst->empty()) return callback(T());
  auto i = lst->back();
  lst->pop_back();
  visit(
      r, i,
      [=](const T& e) {
        if (e.left()) return callback(e.left());
        visit(r, lst, callback, visitor);
      },
      visitor);
}

template class RecursiveRequest<EitherError<void>>;
template class RecursiveRequest<EitherError<IItem>>;

}  // namespace cloudstorage
