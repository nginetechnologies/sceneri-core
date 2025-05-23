// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <Physics/Vehicle/VehicleEngine.h>
#include <ObjectStream/TypeDeclarations.h>
#ifdef JPH_DEBUG_RENDERER
	#include <Renderer/DebugRenderer.h>
#endif // JPH_DEBUG_RENDERER

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_NON_VIRTUAL(VehicleEngineSettings)
{
	JPH_ADD_ATTRIBUTE(VehicleEngineSettings, mMaxTorque)
	JPH_ADD_ATTRIBUTE(VehicleEngineSettings, mMinRPM)
	JPH_ADD_ATTRIBUTE(VehicleEngineSettings, mMaxRPM)
	JPH_ADD_ATTRIBUTE(VehicleEngineSettings, mNormalizedTorque)
}

VehicleEngineSettings::VehicleEngineSettings()
{
	mNormalizedTorque.Reserve(3);
	mNormalizedTorque.AddPoint(0.0f, 0.8f);
	mNormalizedTorque.AddPoint(0.66f, 1.0f);
	mNormalizedTorque.AddPoint(1.0f, 0.8f);
}

void VehicleEngineSettings::SaveBinaryState(StreamOut &inStream) const
{
	inStream.Write(mMaxTorque);
	inStream.Write(mMinRPM);
	inStream.Write(mMaxRPM);
	mNormalizedTorque.SaveBinaryState(inStream);
}

void VehicleEngineSettings::RestoreBinaryState(StreamIn &inStream)
{
	inStream.Read(mMaxTorque);
	inStream.Read(mMinRPM);
	inStream.Read(mMaxRPM);
	mNormalizedTorque.RestoreBinaryState(inStream);
}

void VehicleEngine::UpdateRPM(float inDeltaTime, float inAcceleration)
{
	// Angular damping: dw/dt = -c * w
	// Solution: w(t) = w(0) * e^(-c * t) or w2 = w1 * e^(-c * dt)
	// Taylor expansion of e^(-c * dt) = 1 - c * dt + ...
	// Since dt is usually in the order of 1/60 and c is a low number too this approximation is good enough
	mCurrentRPM *= max(0.0f, 1.0f - mAngularDamping * inDeltaTime);

	// Accelerate engine using torque
	mCurrentRPM += cAngularVelocityToRPM * GetTorque(inAcceleration) * inDeltaTime / mInertia;

	// Clamp RPM
	mCurrentRPM = Clamp(mCurrentRPM, mMinRPM, mMaxRPM);
}

#ifdef JPH_DEBUG_RENDERER

void VehicleEngine::DrawRPM(DebugRenderer *inRenderer, Vec3Arg inPosition, Vec3Arg inForward, Vec3Arg inUp, float inSize, float inShiftDownRPM, float inShiftUpRPM) const
{
	// Function that converts RPM to an angle in radians
	auto rpm_to_angle = [this](float inRPM) { return (-0.75f + 1.5f * (inRPM - mMinRPM) / (mMaxRPM - mMinRPM)) * JPH_PI; };

	// Function to draw part of a pie
	auto draw_pie = [rpm_to_angle, inRenderer, inSize, inPosition, inForward, inUp](float inMinRPM, float inMaxRPM, Color inColor) { 
		inRenderer->DrawPie(inPosition, inSize, inForward, inUp, rpm_to_angle(inMinRPM), rpm_to_angle(inMaxRPM), inColor, DebugRenderer::ECastShadow::Off);
	};

	// Draw segment until inShiftDownRPM
	if (mCurrentRPM < inShiftDownRPM)
	{
		draw_pie(mMinRPM, mCurrentRPM, Color::sRed);
		draw_pie(mCurrentRPM, inShiftDownRPM, Color::sDarkRed);
	}
	else
	{
		draw_pie(mMinRPM, inShiftDownRPM, Color::sRed);
	}

	// Draw segment between inShiftDownRPM and inShiftUpRPM
	if (mCurrentRPM > inShiftDownRPM && mCurrentRPM < inShiftUpRPM)
	{
		draw_pie(inShiftDownRPM, mCurrentRPM, Color::sOrange);
		draw_pie(mCurrentRPM, inShiftUpRPM, Color::sDarkOrange);
	}
	else
	{
		draw_pie(inShiftDownRPM, inShiftUpRPM, mCurrentRPM <= inShiftDownRPM? Color::sDarkOrange : Color::sOrange);
	}

	// Draw segment above inShiftUpRPM
	if (mCurrentRPM > inShiftUpRPM)
	{
		draw_pie(inShiftUpRPM, mCurrentRPM, Color::sGreen);
		draw_pie(mCurrentRPM, mMaxRPM, Color::sDarkGreen);
	}
	else
	{
		draw_pie(inShiftUpRPM, mMaxRPM, Color::sDarkGreen);
	}
}

#endif // JPH_DEBUG_RENDERER

void VehicleEngine::SaveState(StateRecorder &inStream) const
{
	inStream.Write(mCurrentRPM);
}

void VehicleEngine::RestoreState(StateRecorder &inStream)
{
	inStream.Read(mCurrentRPM);
}

JPH_NAMESPACE_END
