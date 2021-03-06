/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "descriptors.h"

#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <android-base/stringprintf.h>
#include <cutils/files.h>
#include <cutils/sockets.h>

#include "init.h"
#include "log.h"
#include "util.h"

DescriptorInfo::DescriptorInfo(const std::string& name, const std::string& type, uid_t uid,
                               gid_t gid, int perm, const std::string& context)
        : name_(name), type_(type), uid_(uid), gid_(gid), perm_(perm), context_(context) {
}

DescriptorInfo::~DescriptorInfo() {
}

std::ostream& operator<<(std::ostream& os, const DescriptorInfo& info) {
  return os << "  descriptors " << info.name_ << " " << info.type_ << " " << std::oct << info.perm_;
}

bool DescriptorInfo::operator==(const DescriptorInfo& other) const {
  return name_ == other.name_ && type_ == other.type_ && key() == other.key();
}

void DescriptorInfo::CreateAndPublish(const std::string& globalContext) const {
  // Create
  const std::string& contextStr = context_.empty() ? globalContext : context_;
  int fd = Create(contextStr);
  if (fd < 0) return;

  // Publish
  std::string publishedName = key() + name_;
  std::for_each(publishedName.begin(), publishedName.end(),
                [] (char& c) { c = isalnum(c) ? c : '_'; });

  std::string val = android::base::StringPrintf("%d", fd);
  add_environment(publishedName.c_str(), val.c_str());

  // make sure we don't close on exec
  fcntl(fd, F_SETFD, 0);
}

void DescriptorInfo::Clean() const {
}

SocketInfo::SocketInfo(const std::string& name, const std::string& type, uid_t uid,
                       gid_t gid, int perm, const std::string& context)
        : DescriptorInfo(name, type, uid, gid, perm, context) {
}

void SocketInfo::Clean() const {
  unlink(android::base::StringPrintf(ANDROID_SOCKET_DIR "/%s", name().c_str()).c_str());
}

int SocketInfo::Create(const std::string& context) const {
  int flags = ((type() == "stream" ? SOCK_STREAM :
                (type() == "dgram" ? SOCK_DGRAM :
                 SOCK_SEQPACKET)));
  return create_socket(name().c_str(), flags, perm(), uid(), gid(), context.c_str());
}

const std::string SocketInfo::key() const {
  return ANDROID_SOCKET_ENV_PREFIX;
}

FileInfo::FileInfo(const std::string& name, const std::string& type, uid_t uid,
                   gid_t gid, int perm, const std::string& context)
        : DescriptorInfo(name, type, uid, gid, perm, context) {
}

int FileInfo::Create(const std::string& context) const {
  int flags = ((type() == "r" ? O_RDONLY :
                (type() == "w" ? (O_WRONLY | O_CREAT) :
                 (O_RDWR | O_CREAT))));
  return create_file(name().c_str(), flags, perm(), uid(), gid(), context.c_str());
}

const std::string FileInfo::key() const {
  return ANDROID_FILE_ENV_PREFIX;
}
