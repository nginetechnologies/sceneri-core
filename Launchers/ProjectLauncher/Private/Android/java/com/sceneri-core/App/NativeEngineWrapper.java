package com.sceneri_core.App;
public class NativeEngineWrapper {
    public native void OnDeviceAdded(int deviceId, long activeAxisIds);
    public native void OnDeviceRemoved(int deviceId);
    public native void OnDeviceChanged(int deviceId);
    public native void SetViewControllerSettings(int scaledTouchSlope, int scaledMinimumScalingSpan);

    public native void OnSignedInWithGoogle(String userIdentifier, String idToken);
    public native void OnSignInWithGoogleFailed();

    public native void OnReceiveUrlIntent(String url);
}
