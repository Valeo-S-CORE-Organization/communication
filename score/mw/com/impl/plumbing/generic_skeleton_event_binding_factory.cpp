/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory_impl.h"

namespace score::mw::com::impl
{

// Define the static mock pointer
IGenericSkeletonEventBindingFactory* GenericSkeletonEventBindingFactory::mock_ = nullptr;

IGenericSkeletonEventBindingFactory& GenericSkeletonEventBindingFactory::instance()
{
    if (mock_)
    {
        return *mock_;
    }

    static GenericSkeletonEventBindingFactoryImpl impl;
    return impl;
}

Result<std::unique_ptr<GenericSkeletonEventBinding>> GenericSkeletonEventBindingFactory::Create(
    SkeletonBase& skeleton_base,
    std::string_view event_name,
    const SizeInfo& size_info) noexcept
{
    return instance().Create(skeleton_base, event_name, size_info);
}

} // namespace score::mw::com::impl