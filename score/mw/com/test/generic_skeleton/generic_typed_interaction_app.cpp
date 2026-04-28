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
#include "score/mw/com/impl/generic_skeleton.h"
#include "score/mw/com/impl/instance_specifier.h"
#include "score/mw/com/impl/proxy_event.h"
#include "score/mw/com/impl/traits.h"
#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"
#include "score/mw/log/logging.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace
{

struct MyEventData
{
    uint64_t counter;
    char padding[56]; // Make it a non-trivial size to expose alignment/size bugs
};

constexpr std::string_view kInstanceSpecifier = "/test/generic/typed/interaction";
constexpr std::string_view kEventName = "MyTestEvent";
constexpr int kSamplesToProcess = 5;

int run_provider()
{
    score::mw::log::LogInfo("GenericSkeletonProvider") << "Starting up.";

    const auto instance_specifier = score::mw::com::impl::InstanceSpecifier::Create(std::string{kInstanceSpecifier}).value();

    const score::mw::com::impl::DataTypeMetaInfo event_meta_info{sizeof(MyEventData), alignof(MyEventData)};
    const std::vector<score::mw::com::impl::EventInfo> events = {{kEventName, event_meta_info}};

    score::mw::com::impl::GenericSkeletonServiceElementInfo create_params;
    create_params.events = events;

    auto skeleton_res = score::mw::com::impl::GenericSkeleton::Create(instance_specifier, create_params);
    if (!skeleton_res.has_value())
    {
        score::mw::log::LogFatal("GenericSkeletonProvider") << "Failed to create skeleton.";
        return 1;
    }
    auto& skeleton = skeleton_res.value();

    if (!skeleton.OfferService().has_value())
    {
        score::mw::log::LogFatal("GenericSkeletonProvider") << "Failed to offer service.";
        return 1;
    }
    score::mw::log::LogInfo("GenericSkeletonProvider") << "Service offered.";

    auto event_it = skeleton.GetEvents().find(std::string(kEventName));
    if (event_it == skeleton.GetEvents().cend())
    {
        score::mw::log::LogFatal("GenericSkeletonProvider") << "Failed to find event " << kEventName << " in skeleton.";
        return 1;
    }
    auto& generic_event = const_cast<score::mw::com::impl::GenericSkeletonEvent&>(event_it->second);

    for (int i = 0; i < kSamplesToProcess; ++i)
    {
        auto sample_res = generic_event.Allocate();
        if (!sample_res.has_value()) {
            score::mw::log::LogFatal("GenericSkeletonProvider") << "Failed to allocate sample.";
            return 1;
        }
        auto* typed_sample = static_cast<MyEventData*>(sample_res.value().Get());
        typed_sample->counter = i;
        generic_event.Send(std::move(sample_res.value()));
        score::mw::log::LogInfo("GenericSkeletonProvider") << "Sent sample " << i;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Give the consumer ample time to connect, subscribe, and read the 
    // samples before we destroy the shared memory pool.
    score::mw::log::LogInfo("GenericSkeletonProvider") << "Finished sending. Waiting for consumer to process...";
    std::this_thread::sleep_for(std::chrono::seconds(2));

    skeleton.StopOfferService();
    score::mw::log::LogInfo("GenericSkeletonProvider") << "Shutting down.";
    return 0;
}

template <typename Trait>
class MyTestService : public Trait::Base
{
  public:
    using Trait::Base::Base;

    typename Trait::template Event<MyEventData> my_test_event_{*this, kEventName};
};
using MyTestServiceProxy = score::mw::com::impl::AsProxy<MyTestService>;

int run_consumer()
{
    score::mw::log::LogInfo("TypedProxyConsumer") << "Starting up.";

    const auto instance_specifier = score::mw::com::impl::InstanceSpecifier::Create(std::string{kInstanceSpecifier}).value();

    auto handles_res = MyTestServiceProxy::FindService(instance_specifier);
    if (!handles_res.has_value() || handles_res.value().empty())
    {
        score::mw::log::LogFatal("TypedProxyConsumer") << "Failed to find service.";
        return 1;
    }

    auto proxy_res = MyTestServiceProxy::Create(handles_res.value()[0]);
    if (!proxy_res.has_value())
    {
        score::mw::log::LogFatal("TypedProxyConsumer") << "Failed to create proxy.";
        return 1;
    }
    auto& proxy = proxy_res.value();
    score::mw::log::LogInfo("TypedProxyConsumer") << "Proxy created.";

    uint64_t received_count = 0;
    uint64_t expected_counter = 0;

    proxy.my_test_event_.Subscribe(kSamplesToProcess);

    while (received_count < kSamplesToProcess)
    {
        proxy.my_test_event_.GetNewSamples([&](score::mw::com::SamplePtr<MyEventData> sample) {
            score::mw::log::LogInfo("TypedProxyConsumer") << "Received sample with counter: " << sample->counter;
            if (sample->counter != expected_counter)
            {
                score::mw::log::LogFatal("TypedProxyConsumer")
                    << "Data validation failed! Expected " << expected_counter << ", got " << sample->counter;
                std::exit(1); // Use exit to fail fast and make the test fail.
            }
            expected_counter++;
            received_count++;
        }, kSamplesToProcess);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    score::mw::log::LogInfo("TypedProxyConsumer") << "Successfully received and validated all samples. Shutting down.";
    return 0;
}

} // namespace

int main(int argc, const char* argv[])
{
    std::string mode;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc)
        {
            mode = argv[++i];
        }
    }

    score::mw::com::runtime::InitializeRuntime(score::mw::com::runtime::RuntimeConfiguration(argc, argv));

    if (mode == "provider")
    {
        return run_provider();
    }
    else if (mode == "consumer")
    {
        return run_consumer();
    }

    score::mw::log::LogFatal("Main") << "Invalid or missing mode. Use --mode provider or --mode consumer.";
    return 1;
}