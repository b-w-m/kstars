/*
    Tests for scheduler job state machine.

    SPDX-FileCopyrightText: 2021 Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_sequencejobstate.h"

#include "Options.h"

TestSequenceJobState::TestSequenceJobState() : QObject() {}


void TestSequenceJobState::testFullParameterSet()
{
    QFETCH(bool, isPreview);
    QFETCH(bool, enforce_rotate);
    QFETCH(bool, enforce_temperature);

    double current_temp = 10.0, target_temp = -10.0;
    double current_angle = 10.0, target_angle = 50;
    // set current and target values
    if (enforce_temperature)
    {
        m_adapter->setCCDTemperature(current_temp);
        m_stateMachine->setTargetCCDTemperature(target_temp);
    }
    if (enforce_rotate)
    {
        m_adapter->setRotatorAngle(current_angle, IPS_OK);
        m_stateMachine->setTargetRotatorAngle(target_angle);
    }

    // start the capture preparation
    m_stateMachine->prepareLightFrameCapture(enforce_temperature, isPreview);
    QVERIFY(m_adapter->isCapturePreparationComplete == !(enforce_temperature | enforce_rotate));
    // now step by step set the values to the target value
    if (enforce_temperature)
        m_adapter->setCCDTemperature(target_temp + 0.5 * Options::maxTemperatureDiff());
    QVERIFY(m_adapter->isCapturePreparationComplete == !enforce_rotate);
    if (enforce_rotate)
        m_adapter->setRotatorAngle(target_angle + 0.5 * Options::astrometryRotatorThreshold() / 60, IPS_OK);
    QVERIFY(m_adapter->isCapturePreparationComplete == true);
}

void TestSequenceJobState::testLazyInitialisation()
{
    QFETCH(bool, isPreview);
    QFETCH(bool, enforce_rotate);
    QFETCH(bool, enforce_temperature);

    // we set current = target so that it is not necessary to update the device values
    // but the state machine needs to ask for the current values
    double current_temp = 10.0, target_temp = current_temp;
    double current_angle = 10.0, target_angle = current_angle;

    // initialize the test processor, but do not inform the state machine
    m_adapter->init(current_temp, current_angle);

    // set target values
    if (enforce_temperature)
        m_stateMachine->setTargetCCDTemperature(target_temp);
    if (enforce_rotate)
        m_stateMachine->setTargetRotatorAngle(target_angle);

    // start the capture preparation
    m_stateMachine->prepareLightFrameCapture(enforce_temperature, isPreview);

    // Since the state machine does not know the current values, it needs to request them.
    // If this happens, the preparation is already done, since we have current = target
    QTRY_VERIFY_WITH_TIMEOUT(m_adapter->isCapturePreparationComplete, 5000);
}

void TestSequenceJobState::testWithProcessor()
{
    TestProcessor *processor = new TestProcessor();

    double current_temp = 10.0, target_temp = -10.0;
    double current_angle = 10.0, target_angle = 50;
    bool isPreview = processor->isPreview;

    // set current values
    m_adapter->setCCDTemperature(current_temp);
    m_adapter->setRotatorAngle(current_angle, IPS_OK);

    // set target values
    m_stateMachine->setTargetCCDTemperature(target_temp);
    m_stateMachine->setTargetRotatorAngle(target_angle);

    // connect the processor
    connect(m_stateMachine, &Ekos::SequenceJobState::setRotatorAngle, processor, &TestProcessor::setRotatorAngle);
    connect(m_stateMachine, &Ekos::SequenceJobState::setCCDTemperature, processor, &TestProcessor::setCCDTemperature);
    connect(m_stateMachine, &Ekos::SequenceJobState::setCCDBatchMode, processor, &TestProcessor::setCCDBatchMode);
    connect(processor, &TestProcessor::newRotatorAngle, m_stateMachine, &Ekos::SequenceJobState::setCurrentRotatorAngle);
    connect(processor, &TestProcessor::newCCDTemperature, m_stateMachine, &Ekos::SequenceJobState::setCurrentCCDTemperature);

    // start the capture preparation
    m_stateMachine->prepareLightFrameCapture(true, isPreview);
    QVERIFY(m_adapter->isCapturePreparationComplete == true);
    // verify if the batch mode has been set
    QVERIFY(processor->isPreview == isPreview);

    // disconnect the processor
    disconnect(m_stateMachine, nullptr, processor, nullptr);
    disconnect(processor, nullptr, m_stateMachine, nullptr);
}

/* *********************************************************************************
 * Test data
 * ********************************************************************************* */

void TestSequenceJobState::testFullParameterSet_data()
{
    QTest::addColumn<bool>("isPreview");           /*!< preview capture? */
    QTest::addColumn<bool>("enforce_rotate");      /*!< enforce rotating? */
    QTest::addColumn<bool>("enforce_temperature"); /*!< enforce temperature? */

    // iterate over all combinations
    for (bool preview : {true, false})
        for (bool rotate : {true, false})
            for (bool temperature : {true, false})
                QTest::newRow(QString("preview=%4 enforce rotate=%1, temperature=%2").arg(rotate).arg(temperature).arg(preview).toLocal8Bit())
                        << preview << rotate << temperature;
}

void TestSequenceJobState::testLazyInitialisation_data()
{
    testFullParameterSet_data();
}

/* *********************************************************************************
 * Test infrastructure
 * ********************************************************************************* */
void TestSequenceJobState::initTestCase()
{
    qDebug() << "initTestCase() started.";
}

void TestSequenceJobState::cleanupTestCase()
{
    qDebug() << "cleanupTestCase() started.";
}

void TestSequenceJobState::init()
{
    QSharedPointer<Ekos::SequenceJobState::CaptureState> captureState;
    captureState.reset(new Ekos::SequenceJobState::CaptureState());
    m_stateMachine = new Ekos::SequenceJobState(captureState);
    // currently all tests are for light frames
    m_stateMachine->setFrameType(FRAME_LIGHT);
    QVERIFY(m_stateMachine->getStatus() == Ekos::JOB_IDLE);
    m_adapter = new TestAdapter();
    QVERIFY(m_adapter->isCapturePreparationComplete == false);
    // forward signals to the sequence job
    connect(m_adapter, &TestAdapter::prepareCapture, m_stateMachine, &Ekos::SequenceJobState::prepareLightFrameCapture);
    connect(m_adapter, &TestAdapter::newRotatorAngle, m_stateMachine, &Ekos::SequenceJobState::setCurrentRotatorAngle);
    connect(m_adapter, &TestAdapter::newCCDTemperature, m_stateMachine, &Ekos::SequenceJobState::setCurrentCCDTemperature);
    // react upon sequence job signals
    connect(m_stateMachine, &Ekos::SequenceJobState::prepareComplete, m_adapter, &TestAdapter::setCapturePreparationComplete);
    connect(m_stateMachine, &Ekos::SequenceJobState::readCurrentState, m_adapter, &TestAdapter::readCurrentState);

}

void TestSequenceJobState::cleanup()
{
    disconnect(m_adapter, nullptr, m_stateMachine, nullptr);
    disconnect(m_stateMachine, nullptr, m_adapter, nullptr);
    delete m_adapter;
    delete m_stateMachine;
    m_adapter = nullptr;
    m_stateMachine = nullptr;
}



void TestAdapter::init(double temp, double angle)
{
    m_ccdtemperature = temp;
    m_rotatorangle = angle;
}

void TestAdapter::setCCDTemperature(double value)
{
    // emit only a new value if it is not too close to the last one
    if (std::abs(m_ccdtemperature - value) > Options::maxTemperatureDiff() / 10)
        emit newCCDTemperature(value);
    // remember it
    m_ccdtemperature = value;
}

void TestAdapter::setRotatorAngle(double value, IPState state)
{
    // emit only a new value if it is not too close to the last one
    if (std::abs(m_rotatorangle - value) > 0.1)
        emit newRotatorAngle(value, state);
    // remember it
    m_rotatorangle = value;
}

void TestAdapter::readCurrentState(Ekos::CaptureState state)
{
    // signal the current device value
    switch (state)
    {
        case Ekos::CAPTURE_SETTING_TEMPERATURE:
            emit newCCDTemperature(m_ccdtemperature);
            break;
        case Ekos::CAPTURE_SETTING_ROTATOR:
            emit newRotatorAngle(m_rotatorangle, IPS_OK);
            break;
        default:
            // do nothing
            break;
    }
}

QTEST_GUILESS_MAIN(TestSequenceJobState)
