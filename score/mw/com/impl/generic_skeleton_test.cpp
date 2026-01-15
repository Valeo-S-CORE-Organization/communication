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

#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/runtime_mock.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory_mock.h"
#include "score/mw/com/impl/plumbing/generic_skeleton_event_binding_factory.h" 
#include "score/mw/com/impl/bindings/mock_binding/generic_skeleton_event.h"

// Lifecycle and Runtime mocks
#include "score/mw/com/impl/i_binding_runtime.h"
#include "score/mw/com/impl/service_discovery_client_mock.h" 
#include "score/mw/com/impl/tracing/i_binding_tracing_runtime.h"

#include "score/mw/com/impl/test/dummy_instance_identifier_builder.h"
#include "score/mw/com/impl/test/runtime_mock_guard.h"
#include "score/mw/com/impl/test/binding_factory_resources.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace score::mw::com::impl
{
namespace
{

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::NiceMock;
using ::testing::Field;
using ::testing::AllOf;
using ::testing::Invoke;

// --- 1. Define IServiceDiscoveryMock (COMPLETED) ---
class IServiceDiscoveryMock : public IServiceDiscovery
{
public:
    // Methods used in test
    MOCK_METHOD(ResultBlank, OfferService, (InstanceIdentifier), (noexcept, override));
    MOCK_METHOD(ResultBlank, StopOfferService, (InstanceIdentifier), (noexcept, override));

    // Missing methods required to instantiate the class
    MOCK_METHOD(ResultBlank, StopOfferService, (InstanceIdentifier, QualityTypeSelector), (noexcept, override));
    
    MOCK_METHOD(Result<FindServiceHandle>, StartFindService, (FindServiceHandler<HandleType>, InstanceSpecifier), (noexcept, override));
    MOCK_METHOD(Result<FindServiceHandle>, StartFindService, (FindServiceHandler<HandleType>, InstanceIdentifier), (noexcept, override));
    MOCK_METHOD(Result<FindServiceHandle>, StartFindService, (FindServiceHandler<HandleType>, EnrichedInstanceIdentifier), (noexcept, override));
    
    MOCK_METHOD(ResultBlank, StopFindService, (const FindServiceHandle), (noexcept, override));
    
    MOCK_METHOD(Result<ServiceHandleContainer<HandleType>>, FindService, (InstanceIdentifier), (noexcept, override));
    MOCK_METHOD(Result<ServiceHandleContainer<HandleType>>, FindService, (InstanceSpecifier), (noexcept, override));
};

// --- 2. FULLY DEFINED Tracing Mock ---
class IBindingTracingRuntimeMock : public tracing::IBindingTracingRuntime {
public:
    using TracingSlotSizeType = tracing::IBindingTracingRuntime::TracingSlotSizeType;
    using TraceContextId = tracing::IBindingTracingRuntime::TraceContextId;

    MOCK_METHOD(tracing::ServiceElementTracingData, RegisterServiceElement, (TracingSlotSizeType), (noexcept, override));
    MOCK_METHOD(bool, RegisterWithGenericTraceApi, (), (noexcept, override));
    MOCK_METHOD(score::analysis::tracing::TraceClientId, GetTraceClientId, (), (const, noexcept, override));
    MOCK_METHOD(void, SetDataLossFlag, (const bool), (noexcept, override));
    MOCK_METHOD(bool, GetDataLossFlag, (), (const, noexcept, override));
    
    MOCK_METHOD(void, RegisterShmObject, (const tracing::ServiceElementInstanceIdentifierView&, score::analysis::tracing::ShmObjectHandle, void* const), (noexcept, override));
    MOCK_METHOD(void, UnregisterShmObject, (const tracing::ServiceElementInstanceIdentifierView&), (noexcept, override));
    
    MOCK_METHOD(score::cpp::optional<score::analysis::tracing::ShmObjectHandle>, GetShmObjectHandle, (const tracing::ServiceElementInstanceIdentifierView&), (const, noexcept, override));
    MOCK_METHOD(score::cpp::optional<void*>, GetShmRegionStartAddress, (const tracing::ServiceElementInstanceIdentifierView&), (const, noexcept, override));
    
    MOCK_METHOD(void, CacheFileDescriptorForReregisteringShmObject, (const tracing::ServiceElementInstanceIdentifierView&, const score::memory::shared::ISharedMemoryResource::FileDescriptor, void* const), (noexcept, override));
    
    MOCK_METHOD((score::cpp::optional<std::pair<score::memory::shared::ISharedMemoryResource::FileDescriptor, void*>>), GetCachedFileDescriptorForReregisteringShmObject, (const tracing::ServiceElementInstanceIdentifierView&), (const, noexcept, override));
    MOCK_METHOD(void, ClearCachedFileDescriptorForReregisteringShmObject, (const tracing::ServiceElementInstanceIdentifierView&), (noexcept, override));
    
    MOCK_METHOD(score::analysis::tracing::ServiceInstanceElement, ConvertToTracingServiceInstanceElement, (const tracing::ServiceElementInstanceIdentifierView), (const, override));
    
    MOCK_METHOD(std::optional<TraceContextId>, EmplaceTypeErasedSamplePtr, (tracing::TypeErasedSamplePtr, const tracing::ServiceElementTracingData), (noexcept, override));
    MOCK_METHOD(void, ClearTypeErasedSamplePtr, (const TraceContextId), (noexcept, override));
    MOCK_METHOD(void, ClearTypeErasedSamplePtrs, (const tracing::ServiceElementTracingData&), (noexcept, override));
};

// --- 3. Define the BindingRuntime Mock ---
class IBindingRuntimeMock : public IBindingRuntime {
public:
    MOCK_METHOD(IServiceDiscoveryClient&, GetServiceDiscoveryClient, (), (noexcept, override));
    MOCK_METHOD(BindingType, GetBindingType, (), (const, noexcept, override));
    MOCK_METHOD(tracing::IBindingTracingRuntime*, GetTracingRuntime, (), (noexcept, override));
};

class GenericSkeletonTest : public ::testing::Test
{
  public:
    GenericSkeletonTest()
    {
        // --- Setup Factory to return a NEW mock binding every time Create is called ---
        ON_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(_))
            .WillByDefault(Invoke([this](const auto&) {
                auto mock = std::make_unique<NiceMock<mock_binding::Skeleton>>();
                this->skeleton_binding_mock_ = mock.get();
                
                ON_CALL(*mock, PrepareOffer(_, _, _)).WillByDefault(Return(score::Blank{}));
                ON_CALL(*mock, PrepareStopOffer(_)).WillByDefault(Return());
                
                return mock;
            }));

        // --- Setup Runtime Mocks ---
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetBindingRuntime(BindingType::kLoLa))
            .WillByDefault(Return(&binding_runtime_mock_));
        
        ON_CALL(binding_runtime_mock_, GetServiceDiscoveryClient())
            .WillByDefault(ReturnRef(service_discovery_client_mock_));

        // **FIX: Return the IServiceDiscoveryMock**
        ON_CALL(runtime_mock_guard_.runtime_mock_, GetServiceDiscovery())
             .WillByDefault(ReturnRef(service_discovery_mock_));

        ON_CALL(binding_runtime_mock_, GetBindingType())
            .WillByDefault(Return(BindingType::kLoLa));

        ON_CALL(binding_runtime_mock_, GetTracingRuntime())
            .WillByDefault(Return(&binding_tracing_runtime_mock_));

        ON_CALL(binding_tracing_runtime_mock_, RegisterWithGenericTraceApi())
            .WillByDefault(Return(true));

        // Ensure Service Discovery actions succeed
        ON_CALL(service_discovery_mock_, OfferService(_))
            .WillByDefault(Return(score::Blank{}));
        ON_CALL(service_discovery_mock_, StopOfferService(_))
            .WillByDefault(Return(score::Blank{}));
            
        ON_CALL(service_discovery_client_mock_, OfferService(_))
            .WillByDefault(Return(score::Blank{}));
        ON_CALL(service_discovery_client_mock_, StopOfferService(_, _))
            .WillByDefault(Return(score::Blank{}));

        GenericSkeletonEventBindingFactory::mock_ = &generic_event_binding_factory_mock_;
    }

    ~GenericSkeletonTest() override
    {
        GenericSkeletonEventBindingFactory::mock_ = nullptr;
    }

    RuntimeMockGuard runtime_mock_guard_{};
    SkeletonBindingFactoryMockGuard skeleton_binding_factory_mock_guard_{};
    
    NiceMock<IBindingRuntimeMock> binding_runtime_mock_{};
    NiceMock<ServiceDiscoveryClientMock> service_discovery_client_mock_{};
    NiceMock<IServiceDiscoveryMock> service_discovery_mock_{}; // Now fully implemented
    NiceMock<IBindingTracingRuntimeMock> binding_tracing_runtime_mock_{};

    NiceMock<GenericSkeletonEventBindingFactoryMock> generic_event_binding_factory_mock_{};
    
    mock_binding::Skeleton* skeleton_binding_mock_{nullptr};
    
    DummyInstanceIdentifierBuilder dummy_instance_identifier_builder_{};
};

TEST_F(GenericSkeletonTest, CreateWithInstanceSpecifier)
{
    auto instance_specifier = InstanceSpecifier::Create(std::string{"a/b/c"}).value();
    auto instance_identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier();

    EXPECT_CALL(runtime_mock_guard_.runtime_mock_, resolve(instance_specifier))
        .WillOnce(Return(std::vector<InstanceIdentifier>{instance_identifier}));

    auto skeleton_result = GenericSkeleton::Create(instance_specifier);
    ASSERT_TRUE(skeleton_result.has_value());
}

TEST_F(GenericSkeletonTest, CreateWithInstanceIdentifier)
{
    auto instance_identifier = dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier();
    auto skeleton_result = GenericSkeleton::Create(instance_identifier);
    ASSERT_TRUE(skeleton_result.has_value());
}

TEST_F(GenericSkeletonTest, AddEventSuccess)
{
    auto skeleton = GenericSkeleton::Create(dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier()).value();
    const SizeInfo size_info{16, 8};    
    
    auto mock_binding = std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();

    EXPECT_CALL(generic_event_binding_factory_mock_, 
        Create(testing::Ref(skeleton), "MyEvent", 
            AllOf(Field(&SizeInfo::size, size_info.size), Field(&SizeInfo::alignment, size_info.alignment))
        )
    ).WillOnce(Return(ByMove(std::move(mock_binding))));

    auto event_result = skeleton.AddEvent("MyEvent", size_info);
    ASSERT_TRUE(event_result.has_value());
}

TEST_F(GenericSkeletonTest, AddEventTwiceFails)
{
    auto skeleton = GenericSkeleton::Create(dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier()).value();
    const SizeInfo size_info{16, 8};

    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, "MyEvent", _))
        .WillOnce(Return(ByMove(std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>())));

    ASSERT_TRUE(skeleton.AddEvent("MyEvent", size_info).has_value());

    auto second_result = skeleton.AddEvent("MyEvent", size_info);
    ASSERT_FALSE(second_result.has_value());
    EXPECT_EQ(second_result.error(), ComErrc::kServiceElementAlreadyExists);
}

TEST_F(GenericSkeletonTest, OfferAndStopOfferService)
{
    auto skeleton = GenericSkeleton::Create(dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier()).value();
    ASSERT_NE(skeleton_binding_mock_, nullptr); 

    const SizeInfo size_info{16, 8};
    auto mock_event_binding = std::make_unique<NiceMock<mock_binding::GenericSkeletonEvent>>();
    auto* mock_event_binding_ptr = mock_event_binding.get();
    
    EXPECT_CALL(generic_event_binding_factory_mock_, Create(_, "MyEvent", _))
        .WillOnce(Return(ByMove(std::move(mock_event_binding))));
    skeleton.AddEvent("MyEvent", size_info);

    // 1. Offer Service
    EXPECT_CALL(*skeleton_binding_mock_, PrepareOffer(_, _, _)).WillOnce(Return(score::Blank{}));
    EXPECT_CALL(*mock_event_binding_ptr, PrepareOffer()).WillOnce(Return(score::Blank{}));
    EXPECT_CALL(service_discovery_mock_, OfferService(_)).WillOnce(Return(score::Blank{}));

    auto offer_result = skeleton.OfferService();
    EXPECT_TRUE(offer_result.has_value());

    // 2. Stop Service
    EXPECT_CALL(*mock_event_binding_ptr, PrepareStopOffer());
    EXPECT_CALL(*skeleton_binding_mock_, PrepareStopOffer(_)).WillOnce(Return());
    
    skeleton.StopOfferService();
}

TEST_F(GenericSkeletonTest, CreateFailsIfBindingFails)
{
    EXPECT_CALL(skeleton_binding_factory_mock_guard_.factory_mock_, Create(_)).WillOnce(Return(ByMove(nullptr)));
    auto skeleton_result = GenericSkeleton::Create(dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier());
    ASSERT_FALSE(skeleton_result.has_value());
    EXPECT_EQ(skeleton_result.error(), ComErrc::kBindingFailure);
}

TEST_F(GenericSkeletonTest, AddEventAfterOfferServiceFails)
{
    auto skeleton = GenericSkeleton::Create(dummy_instance_identifier_builder_.CreateValidLolaInstanceIdentifier()).value();
    const SizeInfo size_info{16, 8};

    EXPECT_CALL(*skeleton_binding_mock_, PrepareOffer(_, _, _)).WillOnce(Return(score::Blank{}));
    ASSERT_TRUE(skeleton.OfferService().has_value());

    auto event_result = skeleton.AddEvent("MyEvent", size_info);

    ASSERT_FALSE(event_result.has_value());
    EXPECT_EQ(event_result.error(), ComErrc::kServiceInstanceAlreadyOffered);
}

} // namespace
} // namespace score::mw::com::impl