#!/bin/bash

while getopts ":r:" opt; do
  case $opt in
    r)
      RELVER=$OPTARG
      if [ $RELVER -ne 23 -a $RELVER -ne 14 ] ; then
        echo "Only supported versions are Android 14 and 23"
        exit 1
      fi
      ;;
    \?)
      echo "Usage $0 -r <Android release number>" >&2
      exit 1
      ;;
    :)
      echo "Option -$OPTARG requires an argument." >&2
      exit 1
      ;;
  esac
done

if [ ! -f jni/Application.mk.r$RELVER ] ; then
  echo "Android version $RELVER is not supperted."
  exit 1
else
  cd jni
  ln -sf Application.mk.r$RELVER Application.mk
  cd ..
fi

if [ -f project.properties.r$RELVER ] ; then
  ln -sf project.properties.r$RELVER project.properties
fi
