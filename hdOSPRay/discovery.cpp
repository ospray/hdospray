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
#include "pxr/usd/ndr/declare.h"
#include "pxr/usd/ndr/discoveryPlugin.h"
#include "pxr/usd/ndr/filesystemDiscoveryHelpers.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"
#include <algorithm>
#include <cctype>
#include <map>

// PXR_NAMESPACE_OPEN_SCOPE
PXR_NAMESPACE_USING_DIRECTIVE

namespace {

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((discoveryType1, "OspPrincipled"))
    ((discoveryType2, "OspCarPaint"))
    ((discoveryType3, "OspLuminous"))
);

// Maps a nodedef name to its NdrNode name.
using _NameMapping = std::map<std::string, std::string>;

// Return the Ndr name for a nodedef name.
std::string
_ChooseName(const std::string& nodeDefName, const _NameMapping& nameMapping)
{
    auto i = nameMapping.find(nodeDefName);
    return i == nameMapping.end() ? nodeDefName : i->second;
}

} // anonymous namespace

/// Discovers nodes in MaterialX files.
class HdOSPRayDiscoveryPlugin : public NdrDiscoveryPlugin {
public:
    HdOSPRayDiscoveryPlugin();
    ~HdOSPRayDiscoveryPlugin() override = default;

    /// Discover all of the nodes that appear within the the search paths
    /// provided and match the extensions provided.
    NdrNodeDiscoveryResultVec DiscoverNodes(const Context&) override;

    /// Gets the paths that this plugin is searching for nodes in.
    const NdrStringVec& GetSearchURIs() const override;

private:
    /// The paths (abs) indicating where the plugin should search for nodes.
    NdrStringVec _customSearchPaths;
    NdrStringVec _allSearchPaths;
};

HdOSPRayDiscoveryPlugin::HdOSPRayDiscoveryPlugin()
{
}

NdrNodeDiscoveryResultVec
HdOSPRayDiscoveryPlugin::DiscoverNodes(const Context& context)
{
    NdrNodeDiscoveryResultVec result;
    std::vector<TfToken> tkns = {TfToken("OspPrincipled"),
        TfToken("OspCarPaint"), TfToken("OspLuminous")};
    for (auto& tkn : tkns) {
        result.emplace_back(
            tkn, //id
            NdrVersion(1), //version
            tkn.GetString(), //name
            tkn, //family
            tkn, //discovery type
            tkn, //source type
            std::string(), //uri
            std::string()); //resolved uri
    }

    return result;
}

const NdrStringVec&
HdOSPRayDiscoveryPlugin::GetSearchURIs() const
{
    return _allSearchPaths;
}

NDR_REGISTER_DISCOVERY_PLUGIN(HdOSPRayDiscoveryPlugin)

// PXR_NAMESPACE_CLOSE_SCOPE
