package com.scenericore.App;

import static com.google.android.libraries.identity.googleid.GoogleIdTokenCredential.TYPE_GOOGLE_ID_TOKEN_CREDENTIAL;

import android.content.Context;
import android.content.Intent;
import android.hardware.input.InputManager;
import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.util.Log;
import android.view.InputDevice;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import com.google.androidgamesdk.GameActivity;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.Executors;
import androidx.credentials.ClearCredentialStateRequest;
import androidx.credentials.Credential;
import androidx.credentials.CredentialManager;
import androidx.credentials.CredentialManagerCallback;
import androidx.credentials.CustomCredential;
import androidx.credentials.GetCredentialRequest;
import androidx.credentials.GetCredentialResponse;
import androidx.credentials.exceptions.ClearCredentialException;
import androidx.credentials.exceptions.GetCredentialException;
import com.google.android.libraries.identity.googleid.GetGoogleIdOption;
import com.google.android.libraries.identity.googleid.GetSignInWithGoogleOption;
import com.google.android.libraries.identity.googleid.GoogleIdTokenCredential;

import org.jetbrains.annotations.NotNull;

import com.appsflyer.AppsFlyerLib;
import com.appsflyer.deeplink.DeepLinkResult;
import com.appsflyer.deeplink.DeepLinkListener;

import io.sentry.Sentry;
import io.sentry.SentryLevel;
import io.sentry.SentryOptions;
import io.sentry.android.core.SentryAndroid;

public class NativeGameActivity extends GameActivity implements InputManager.InputDeviceListener {

    private static final int MAX_AXIS_COUNT = 48;

    private Context context;
    private com.scenericore.App.NativeEngineWrapper engineWrapper;

    private CredentialManager credentialManager;

    public static native String GetSentryDSN();
    public static native String GetSentryRelease();
    public static native String GetSentryDistribution();
    public static native String GetSentryEnvironment();

    public static native String GetSignInWithGoogleServerClientId();

    @Override
    protected void onCreate(Bundle instance) {
        super.onCreate(instance);

        System.loadLibrary("Editor");

        context = this;

        {
            String distribution = GetSentryDistribution();
            if (!distribution.equals("local"))
            {
                SentryAndroid.init(context, options -> {
                    options.setDsn(GetSentryDSN());
                    options.setRelease(GetSentryRelease());
                    options.setDist(distribution);
                    options.setEnvironment(GetSentryEnvironment());

                    options.setAttachScreenshot(true);
                    options.setSampleRate(1.0);
                    options.setProfilesSampleRate(1.0);
                    options.setTracesSampleRate(1.0);
                    options.setEnableAppStartProfiling(true);
                    });
            }
        }

#if INCLUDE_APPSFLYER
        AppsFlyerLib.getInstance().init("", null, this);
        AppsFlyerLib.getInstance().start(this);

        AppsFlyerLib.getInstance().subscribeForDeepLink(new DeepLinkListener() {
            @Override
            public void onDeepLinking(DeepLinkResult deepLinkResult) {
                if (deepLinkResult.getStatus() == DeepLinkResult.Status.FOUND) {
                    String deepLinkValue = deepLinkResult.getDeepLink().getDeepLinkValue();
                    engineWrapper.OnReceiveUrlIntent(deepLinkValue);
                }
            }
        });
#endif

        engineWrapper = new com.scenericore.App.NativeEngineWrapper();

        credentialManager = CredentialManager.create(context);

        ViewConfiguration vc = ViewConfiguration.get(context);
        engineWrapper.SetViewControllerSettings(vc.getScaledTouchSlop(), vc.getScaledMinimumScalingSpan());

        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_FULLSCREEN);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }

        handleIntent(getIntent());
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        handleIntent(intent);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            View decorView = getWindow().getDecorView();
            decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                    View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                    View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                    View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN);
        }
    }

    @Override
    public void onPause() {
        super.onPause();

        InputManager inputManager = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
        inputManager.unregisterInputDeviceListener(this);
    }

    @Override
    public void onResume() {
        super.onResume();

        InputManager inputManager = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
        inputManager.registerInputDeviceListener(this, null);

        InitializeGameControllerDevices();
    }

    @Override
    public void onInputDeviceAdded(int deviceId) {
        InputManager inputManager = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
        InputDevice inputDevice = inputManager.getInputDevice(deviceId);
        long activeAxisIds = GetActiveAxisIds(inputDevice);
        engineWrapper.OnDeviceAdded(deviceId, activeAxisIds);
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        engineWrapper.OnDeviceRemoved(deviceId);
    }

    @Override
    public void onInputDeviceChanged(int deviceId) {
        engineWrapper.OnDeviceChanged(deviceId);
    }

    private void InitializeGameControllerDevices() {
        InputManager inputManager = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
        ArrayList<Integer> gameControllerIds = GetGameControllerIds();
        for (int gameControllerId : gameControllerIds) {
            InputDevice inputDevice = inputManager.getInputDevice(gameControllerId);
            long activeAxisIds = GetActiveAxisIds(inputDevice);
            engineWrapper.OnDeviceAdded(gameControllerId, activeAxisIds);
        }
    }

    public ArrayList<Integer> GetGameControllerIds() {
        ArrayList<Integer> gameControllerDeviceIds = new ArrayList<>();
        int[] deviceIds = InputDevice.getDeviceIds();
        for (int deviceId : deviceIds) {
            InputDevice dev = InputDevice.getDevice(deviceId);
            int sources = dev.getSources();

            if (((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD)) {
                if (!gameControllerDeviceIds.contains(deviceId)) {
                    gameControllerDeviceIds.add(deviceId);
                }
            }
        }
        return gameControllerDeviceIds;
    }

    public long GetActiveAxisIds(InputDevice inputDevice) {
        long activeAxisIds = 0;
        List<InputDevice.MotionRange> motionRanges = inputDevice.getMotionRanges();
        for (InputDevice.MotionRange motionRange : motionRanges) {
            int axisIndex = motionRange.getAxis();
            if (axisIndex >= 0 && axisIndex < MAX_AXIS_COUNT) {
                int axisSource = motionRange.getSource();
                if (axisSource == InputDevice.SOURCE_JOYSTICK || axisSource == InputDevice.SOURCE_GAMEPAD) {
                    activeAxisIds |= (1 << axisIndex);
                }
            }
        }
        return activeAxisIds;
    }

    public long GetRuntimeMemorySize() {
        return Runtime.getRuntime().freeMemory();
    }

    public void SignInWithGoogleCached() {
        GetGoogleIdOption getGoogleIdOption =
                new GetGoogleIdOption.Builder()
                        .setServerClientId(GetSignInWithGoogleServerClientId())
                        .setFilterByAuthorizedAccounts(true)
                        .setRequestVerifiedPhoneNumber(false)
                        .setAutoSelectEnabled(true)
                        .build();

        GetCredentialRequest getCredentialRequest = new GetCredentialRequest.Builder()
                .addCredentialOption(getGoogleIdOption)
                .build();

        credentialManager.getCredentialAsync(this, getCredentialRequest,
                new CancellationSignal(),
                Executors.newSingleThreadExecutor(),
                new CredentialManagerCallback<GetCredentialResponse, GetCredentialException>() {
                    @Override
                    public void onResult(GetCredentialResponse getCredentialResponse) {
                        Credential credential = getCredentialResponse.getCredential();
                        if (credential.getType().equals("com.google.android.libraries.identity.googleid.TYPE_GOOGLE_ID_TOKEN_CREDENTIAL")) {
                            GoogleIdTokenCredential googleIdTokenCredential =
                                    GoogleIdTokenCredential.createFrom(credential.getData());

                            engineWrapper.OnSignedInWithGoogle(googleIdTokenCredential.getId(), googleIdTokenCredential.getIdToken());
                        } else {
                            engineWrapper.OnSignInWithGoogleFailed();
                        }
                    }

                    @Override
                    public void onError(GetCredentialException e) {
                        engineWrapper.OnSignInWithGoogleFailed();
                    }
                });
    }

    public void SignInWithGoogle() {
        GetSignInWithGoogleOption getSignInWithGoogleOption =
                new GetSignInWithGoogleOption.Builder(GetSignInWithGoogleServerClientId())
                        .build();

        GetCredentialRequest getCredentialRequest = new GetCredentialRequest.Builder()
                .addCredentialOption(getSignInWithGoogleOption)
                .build();

        credentialManager.getCredentialAsync(this, getCredentialRequest,
                new CancellationSignal(),
                Executors.newSingleThreadExecutor(),
                new CredentialManagerCallback<GetCredentialResponse, GetCredentialException>() {
                    @Override
                    public void onResult(GetCredentialResponse getCredentialResponse) {
                        Credential credential = getCredentialResponse.getCredential();
                        if (credential.getType().equals("com.google.android.libraries.identity.googleid.TYPE_GOOGLE_ID_TOKEN_CREDENTIAL")) {
                            GoogleIdTokenCredential googleIdTokenCredential =
                                    GoogleIdTokenCredential.createFrom(credential.getData());

                            engineWrapper.OnSignedInWithGoogle(googleIdTokenCredential.getId(), googleIdTokenCredential.getIdToken());
                        } else {
                            engineWrapper.OnSignInWithGoogleFailed();
                        }
                    }

                    @Override
                    public void onError(GetCredentialException e) {
                        engineWrapper.OnSignInWithGoogleFailed();
                    }
                });
    }

    public void SignOutFromAllProviders() {
        ClearCredentialStateRequest request = new ClearCredentialStateRequest();
        credentialManager.clearCredentialStateAsync(
            request,
            new CancellationSignal(),
            Executors.newSingleThreadExecutor(),
            new CredentialManagerCallback<Void, ClearCredentialException>() {
                @Override
                public void onResult(Void unused) {
                }

                @Override
                public void onError(@NotNull ClearCredentialException e) {
                }
            });
    }

    private void handleIntent(Intent intent) {
        if (Objects.equals(intent.getAction(), Intent.ACTION_VIEW)) {
            String uriString = intent.getData().getPath();
            engineWrapper.OnReceiveUrlIntent(uriString);
        }
    }
}
