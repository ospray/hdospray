// Copyright 2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

//
// Copyright 2018 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "pxr/pxr.h"
#include "pxr/usd/ndr/debugCodes.h"
#include "pxr/usd/ndr/node.h"
#include "pxr/usd/ndr/nodeDiscoveryResult.h"
#include "pxr/usd/ndr/parserPlugin.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/sdr/shaderNode.h"
#include "pxr/usd/sdr/shaderProperty.h"
#include "pxr/usd/usdUtils/pipeline.h"
#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/fileUtils.h"
#include "pxr/base/tf/pathUtils.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"

PXR_NAMESPACE_USING_DIRECTIVE
namespace {

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((discoveryType1, "OspPrincipled"))
    ((discoveryType2, "OspCarPaint"))
    ((discoveryType3, "OspLuminous"))
    ((sourceType, ""))
);

// This environment variable lets users override the name of the primary
// UV set that MaterialX should look for.  If it's empty, it uses the USD
// default, "st".
TF_DEFINE_ENV_SETTING(USDMTLX_PRIMARY_UV_NAME, "",
    "The name usdMtlx should use to reference the primary UV set.");

static const std::string _GetPrimaryUvSetName()
{
    static const std::string env = TfGetEnvSetting(USDMTLX_PRIMARY_UV_NAME);
    if (env.empty()) {
        return UsdUtilsGetPrimaryUVSetName().GetString();
    }
    return env;
}

// A builder for shader nodes.  We find it convenient to build the
// arguments to SdrShaderNode across multiple functions.  This type
// holds the arguments.
struct ShaderBuilder {
public:
    ShaderBuilder(const NdrNodeDiscoveryResult& discoveryResult)
        : discoveryResult(discoveryResult)
        , valid(true) 
        , metadata(discoveryResult.metadata) { }

    void SetInvalid()              { valid = false; }
    explicit operator bool() const { return valid; }
    bool operator !() const        { return !valid; }

    NdrNodeUniquePtr Build()
    {
        if (!*this) {
            return NdrParserPlugin::GetInvalidNode(discoveryResult);
        }

        return NdrNodeUniquePtr(
                new SdrShaderNode(discoveryResult.identifier,
                                  discoveryResult.version,
                                  discoveryResult.name,
                                  discoveryResult.family,
                                  context,
                                  discoveryResult.sourceType,
                                  definitionURI,
                                  implementationURI,
                                  std::move(properties),
                                  std::move(metadata)));
    }

    void AddPropertyNameRemapping(const std::string& from,
                                  const std::string& to)
    {
        if (from != to) {
            _propertyNameRemapping[from] = to;
        }
    }

public:
    const NdrNodeDiscoveryResult& discoveryResult;
    bool valid;

    std::string definitionURI;
    std::string implementationURI;
    TfToken context;
    NdrPropertyUniquePtrVec properties;
    NdrTokenMap metadata;

private:
    std::map<std::string, std::string> _propertyNameRemapping;
};

} // anonymous namespace

/// Parses nodes in MaterialX files.
class HdOSPRayParserPlugin : public NdrParserPlugin {
public:
    HdOSPRayParserPlugin() = default;
    ~HdOSPRayParserPlugin() override = default;

    NdrNodeUniquePtr Parse(
        const NdrNodeDiscoveryResult& discoveryResult) override;
    const NdrTokenVec& GetDiscoveryTypes() const override;
    const TfToken& GetSourceType() const override;
};

NdrNodeUniquePtr
HdOSPRayParserPlugin::Parse(
    const NdrNodeDiscoveryResult& discoveryResult)
{
    ShaderBuilder builder(discoveryResult);
    return builder.Build();
}

const NdrTokenVec&
HdOSPRayParserPlugin::GetDiscoveryTypes() const
{
    static const NdrTokenVec discoveryTypes = {
        _tokens->discoveryType1, _tokens->discoveryType2,
        _tokens->discoveryType3
    };
    return discoveryTypes;
}

const TfToken&
HdOSPRayParserPlugin::GetSourceType() const
{
    return _tokens->sourceType;
}

NDR_REGISTER_PARSER_PLUGIN(HdOSPRayParserPlugin)

// PXR_NAMESPACE_CLOSE_SCOPE
