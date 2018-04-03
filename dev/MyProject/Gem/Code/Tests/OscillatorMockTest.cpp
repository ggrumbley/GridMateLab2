#include "MyProject_precompiled.h"
#include <AzTest/AzTest.h>
#include <Tests/TestTypes.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Component/ComponentApplication.h>
#include <OscillatorComponent.h>

using namespace AZ;
using namespace AZStd;
using namespace MyProject;
using namespace ::testing;

/**
 * \brief Pretend to be a TransformComponent
 */
class MockTransformComponent
    : public AZ::Component
    , public AZ::TransformBus::Handler
{
public:
    // be sure this guid is unique, avoid copy-paste errors!
    AZ_COMPONENT(MockTransformComponent,
        "{7E8087BD-46DA-4708-ADB0-08D7812CA49F}");

    static void Reflect(ReflectContext*) {}

    // OscillatorComponent will be calling these methods
    MOCK_METHOD1(SetWorldTranslation, void (const AZ::Vector3&));
    MOCK_METHOD0(GetWorldTranslation, AZ::Vector3 ());

    // Mocking out pure virtual methods
    void Activate() override
    {
        BusConnect(GetEntityId());
    }
    void Deactivate() override
    {
        BusDisconnect();
    }
    MOCK_METHOD0(IsStaticTransform, bool ());
    MOCK_METHOD0(IsPositionInterpolated, bool ());
    MOCK_METHOD0(IsRotationInterpolated, bool ());
    MOCK_METHOD0(GetLocalTM, const Transform& ());
    MOCK_METHOD0(GetWorldTM, const Transform& ());
};

class OscillatorMockTest
    : public ::UnitTest::AllocatorsFixture
{
    AZStd::unique_ptr<AZ::SerializeContext> m_sc;
    AZStd::unique_ptr<AZ::ComponentDescriptor> m_md;
    AZStd::unique_ptr<AZ::ComponentDescriptor> m_od;

protected:
    void SetUp() override
    {
        ::UnitTest::AllocatorsFixture::SetUp();

        // register components involved in testing
        m_sc = AZStd::make_unique<AZ::SerializeContext>();
        m_md.reset(MockTransformComponent::CreateDescriptor());
        m_md->Reflect(m_sc.get());
        m_od.reset(OscillatorComponent::CreateDescriptor());
        m_od->Reflect(m_sc.get());

        CreateEntity();
    }

    void CreateEntity()
    {
        // create a test entity
        e = AZStd::make_unique<Entity>();
        // OscillatorComponent is the component we are testing
        e->CreateComponent<OscillatorComponent>();
        // We can mock out Transform and test the interaction
        mock = new NiceMock<MockTransformComponent>();
        e->AddComponent(mock);

        // Bring the entity online
        e->Init();
        e->Activate();
    }

    unique_ptr<Entity> e;
    MockTransformComponent* mock = nullptr;
};

TEST_F(OscillatorMockTest, Calls_SetWorldTranslation)
{
    // setup a return value for GetWorldTranslation()
    ON_CALL(*mock, GetWorldTranslation()).WillByDefault(
        Return(AZ::Vector3::CreateZero()));

    // expect SetWorldTranslation() to be called
    EXPECT_CALL(*mock, SetWorldTranslation(_)).Times(1);

    TickBus::Broadcast(&TickBus::Events::OnTick, 0.1f,
            ScriptTimePoint());
}