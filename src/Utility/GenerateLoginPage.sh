#!/usr/bin/env bash

FILES="
  4shared_login.html
  amazons3_login.html
  animezone_login.html
  local_login.html
  localwinrt_login.html
  mega_login.html
  webdav_login.html

  default_error.html
  default_success.html

  bootstrap.min.css
  bootstrap.min.js
  url.min.js
  style.min.css
  jquery.min.js
  cloud.png
  vlc-blue.png
"

cd $(dirname "$0")

OUTPUT_CPP=$PWD/LoginPage.cpp
OUTPUT_H=$PWD/LoginPage.h

cd ../../bin/cloudbrowser/resources

rm -f $OUTPUT_CPP $OUTPUT_H

touch $OUTPUT_CPP 
for i in $FILES; do
  xxd -i $i >> $OUTPUT_CPP
done

touch $OUTPUT_H
for i in $FILES; do
  VAR_NAME="${i//[.-]/_}"
  echo $VAR_NAME | grep -P -q '^\d.*$'
  if [[ $? -eq 0 ]]; then
    VAR_NAME=__$VAR_NAME
  fi
  echo "extern unsigned char ${VAR_NAME}[];" >> $OUTPUT_H
  echo "extern unsigned int ${VAR_NAME}_len;" >> $OUTPUT_H
done
