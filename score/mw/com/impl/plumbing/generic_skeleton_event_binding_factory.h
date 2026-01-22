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
#ifndef SCORE_MW_COM_IMPL_PLUMBING_GENERIC_SKELETON_EVENT_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_PLUMBING_GENERIC_SKELETON_EVENT_BINDING_FACTORY_H

#include "score/mw/com/impl/i_generic_skeleton_event_binding_factory.h"
#include "score/result/result.h"

#include <memory>
#include <string_view>

namespace score::mw::com::impl
{

class GenericSkeletonEventBindingFactory
{
  public:
    // Hook for tests to inject the mock
    static IGenericSkeletonEventBindingFactory* mock_;

    // The public API called by GenericSkeleton
    static Result<std::unique_ptr<GenericSkeletonEventBinding>> Create(
        SkeletonBase& skeleton_base,
        std::string_view event_name,
        const SizeInfo& size_info) noexcept;

  private:
    // Internal helper to get either the Real Impl or the Mock
    static IGenericSkeletonEventBindingFactory& instance();
};

} // namespace score::mw::com::impl

#endif // SCORE_MW_COM_IMPL_PLUMBING_GENERIC_SKELETON_EVENT_BINDING_FACTORY_H