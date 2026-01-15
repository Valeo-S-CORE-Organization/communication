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
#include "score/mw/com/impl/generic_skeleton_event.h"

// Mocks and Plumbing
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory_mock.h"
#include "score/mw/com/impl/bindings/mock_binding/generic_skeleton_event.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"

#include "score/mw/com/impl/test/binding_factory_resources.h"
// Runtime Mocks (REQUIRED to prevent config file crash)
#include "score/mw/com/impl/i_binding_runtime.h"
#include "score/mw/com/impl/service_discovery_client_mock.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"
#include "score/mw/com/impl/runtime_mock.h"

#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl {
namespace {

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::Invoke;

// --- 1. Define Helper Mocks ---
class IBindingRuntimeMock : public IBindingRuntime {
public:
    MOCK_METHOD(IServiceDiscoveryClient&, GetServiceDiscoveryClient, (), (noexcept, override));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(tracing::IBindingTracingRuntime*, GetTracingRuntime, (), (noexcept, override));
};

class IServiceDiscoveryMock : public IServiceDiscovery {
public:
    MOCK_METHOD(ResultBlank, OfferService, (InstanceIdentifier), (noexcept, override));
    MOCK_METHOD(ResultBlank, StopOfferService, (InstanceIdentifier), (noexcept, override));
    // Minimal required overrides for compilation
    MOCK_METHOD(ResultBlank, StopOfferService, (InstanceIdentifier, QualityTypeSelector), (noexcept, override));
    MOCK_METHOD(Result<FindServiceHandle>, StartFindService, (FindServiceHandler<HandleType>, InstanceSpecifier), (noexcept, override));
    MOCK_METHOD(Result<FindServiceHandle>, StartFindService, (FindServiceHandler<HandleType>, InstanceIdentifier), (noexcept, override));
    MOCK_METHOD(Result<FindServiceHandle>, StartFindService, (FindServiceHandler<HandleType>, EnrichedInstanceIdentifier), (noexcept, override));
    MOCK_METHOD(ResultBlank, StopFindService, (const FindServiceHandle), (noexcept, override));
    MOCK_METHOD(Result<ServiceHandleContainer<HandleType>>, FindService, (InstanceIdentifier), (noexcept, override));
    MOCK_METHOD(Result<ServiceHandleContainer<HandleType>>, FindService, (InstanceSpecifier), (noexcept, override));
};

// --- 2. Test Fixture ---
class GenericSkeletonEventTest : public ::testing::Test {
public:
    // Mocks
    NiceMock<GenericSkeletonEventBindingFactoryMock> generic_event_binding_factory_mock_;
    RuntimeMockGuard runtime_mock_guard_{};
    NiceMock<IBindingRuntimeMock> binding_runtime_mock_{};
    NiceMock<IServiceDiscoveryMock> service_discovery_mock_{};
    NiceMock<ServiceDiscoveryClientMock> service_discovery_client_mock_{};
    
    // Helper to mock the SkeletonBinding
    SkeletonBindingFactoryMockGuard skeleton_binding_factory_mock_guard_{};
    mock_binding::Skeleton* skeleton_binding_mock_{nullptr};

    GenericSkeletonEventTest() {
        // 1. Setup Static Factory Mock
        GenericSkeletonEventBindingFactory::mock_ = &generic_event_binding_factory_mock_;

        // 2. Setup Runtime Mocks (To avoid "Parsing config file failed" crash)
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetBindingRuntime(BindingType::kLoLa))
            .WillByDefault(Return(&binding_runtime_mock_));
        
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetServiceDiscovery())
             .WillByDefault(ReturnRef(service_discovery_mock_));

        ON_CALL(binding_runtime_mock_, GetBindingType())
            .WillByDefault(Return(BindingType::kLoLa));
            
        ON_CALL(binding_runtime_mock_, GetServiceDiscoveryClient())
            .WillByDefault(ReturnRef(service_discovery_client_mock_));

        ON_CALL(service_discovery_mock_, OfferService(_)).WillByDefault(Return(score::Blank{}));

        // 3. Setup Skeleton Binding Mock (So OfferService works)
        ON_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(_))
            .WillByDefault(Invoke([this](const auto&) {
                auto mock = std::make_unique<NiceMock<mock_binding::Skeleton>>();
                this->skeleton_binding_mock_ = mock.get();
                ON_CALL(*mock, PrepareOffer(_, _, _)).WillByDefault(Return(score::Blank{}));
                return mock;
            }));
    }

    ~GenericSkeletonEventTest() override {
        GenericSkeletonEventBindingFactory::mock_ = nullptr;
    }

protected:
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_;
};

// --- 3. Tests ---

TEST_F(GenericSkeletonEventTest, AllocateBeforeOfferFails)
{
    auto skeleton = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier()
    ).value();

    const SizeInfo size_info{16, 8};
    
    // Expect Create call because AddEvent triggers it
    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, "MyEvent", _))
        .WillOnce(Return(ByMove(std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>())));

    auto* event = skeleton.AddEvent("MyEvent", size_info).value();

    auto alloc_result = event->Allocate();
    ASSERT_FALSE(alloc_result.has_value());
    EXPECT_EQ(alloc_result.error(), ComErrc::kNotOffered);
}

TEST_F(GenericSkeletonEventTest, SendBeforeOfferFails)
{
    auto skeleton = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier()
    ).value();

    const SizeInfo size_info{16, 8};

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, "MyEvent", _))
        .WillOnce(Return(ByMove(std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>())));

    auto* event = skeleton.AddEvent("MyEvent", size_info).value();

    SampleAllocateePtr<void> dummy_sample{nullptr};

    auto send_result = event->Send(std::move(dummy_sample));
    ASSERT_FALSE(send_result.has_value());
    EXPECT_EQ(send_result.error(), ComErrc::kNotOffered);
}

TEST_F(GenericSkeletonEventTest, AllocateAndSendSucceedsAfterOffer)
{
    auto skeleton = GenericSkeleton::Create(
        dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier()
    ).value();

    const SizeInfo size_info{16, 8};

    auto mock_event_binding = std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();
    auto* mock_event_binding_ptr = mock_event_binding.get();

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, "MyEvent", _))
        .WillOnce(Return(ByMove(std::move(mock_event_binding))));

    auto* event = skeleton.AddEvent("MyEvent", size_info).value();

    // 1. Offer
    EXPECT_CALL(*mock_event_binding_ptr, PrepareOffer()).WillOnce(Return(score::Blank{}));
    ASSERT_TRUE(skeleton.OfferService().has_value());

    // 2. Allocate
    lola::SampleAllocateePtr<void> dummy_alloc{nullptr};
    EXPECT_CALL(*mock_event_binding_ptr, Allocate())
        .WillOnce(Return(ByMove(std::move(dummy_alloc))));

    auto alloc_result = event->Allocate();
    ASSERT_TRUE(alloc_result.has_value());

    auto send_result = event->Send(std::move(alloc_result.value()));
    ASSERT_TRUE(send_result.has_value());
}
} // namespace
} // namespace score::mw::com::impl