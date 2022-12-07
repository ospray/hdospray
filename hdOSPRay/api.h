// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef HDOSPRAY_API_H
#define HDOSPRAY_API_H

#include <pxr/base/arch/export.h>

#if defined(PXR_STATIC)
#    define HDOSPRAY_API
#    define HDOSPRAY_API_TEMPLATE_CLASS(...)
#    define HDOSPRAY_API_TEMPLATE_STRUCT(...)
#    define HDOSPRAY_LOCAL
#else
#    if defined(HDOSPRAY_EXPORTS)
#        define HDOSPRAY_API ARCH_EXPORT
#        define HDOSPRAY_API_TEMPLATE_CLASS(...)                               \
            ARCH_EXPORT_TEMPLATE(class, __VA_ARGS__)
#        define HDOSPRAY_API_TEMPLATE_STRUCT(...)                              \
            ARCH_EXPORT_TEMPLATE(struct, __VA_ARGS__)
#    else
#        define HDOSPRAY_API ARCH_IMPORT
#        define HDOSPRAY_API_TEMPLATE_CLASS(...)                               \
            ARCH_IMPORT_TEMPLATE(class, __VA_ARGS__)
#        define HDOSPRAY_API_TEMPLATE_STRUCT(...)                              \
            ARCH_IMPORT_TEMPLATE(struct, __VA_ARGS__)
#    endif
#    define HDOSPRAY_LOCAL ARCH_HIDDEN
#endif

#endif // HDOSPRAY_API_H
