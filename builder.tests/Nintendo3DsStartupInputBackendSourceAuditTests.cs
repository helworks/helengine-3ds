namespace helengine.nintendo3ds.builder.tests;

/// <summary>
/// Audits the Nintendo 3DS startup input backend so menu input stays wired to real HID state without per-frame garbage.
/// </summary>
public class Nintendo3DsStartupInputBackendSourceAuditTests {
    /// <summary>
    /// Verifies the backend owns persistent double-buffered gamepad storage and touch history instead of allocating transient frame arrays.
    /// </summary>
    [Fact]
    public void Source_whenBackendCapturesFrames_reusesPersistentStateBuffers() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string headerPath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsStartupInputBackend.hpp");
        string headerCode = File.ReadAllText(headerPath);

        Assert.Contains("Nintendo3DsStartupInputBackend();", headerCode, StringComparison.Ordinal);
        Assert.Contains("~Nintendo3DsStartupInputBackend();", headerCode, StringComparison.Ordinal);
        Assert.Contains("Array<InputGamepadState>* PrimaryCachedGamepads;", headerCode, StringComparison.Ordinal);
        Assert.Contains("Array<InputGamepadState>* SecondaryCachedGamepads;", headerCode, StringComparison.Ordinal);
        Assert.Contains("bool UsePrimaryCachedGamepads;", headerCode, StringComparison.Ordinal);
        Assert.Contains("bool PreviousTouchPressed;", headerCode, StringComparison.Ordinal);
        Assert.Contains("int PreviousTouchX;", headerCode, StringComparison.Ordinal);
        Assert.Contains("int PreviousTouchY;", headerCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the backend captures real Nintendo 3DS HID buttons, circle pad state, and bottom-screen touch coordinates.
    /// </summary>
    [Fact]
    public void Source_whenBackendCapturesFrames_readsHidButtonsCirclePadAndBottomScreenTouch() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsStartupInputBackend.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("#include <3ds.h>", sourceCode, StringComparison.Ordinal);
        Assert.Contains("u32 heldKeys = hidKeysHeld();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("touchPosition touch {};", sourceCode, StringComparison.Ordinal);
        Assert.Contains("circlePosition circlePad {};", sourceCode, StringComparison.Ordinal);
        Assert.Contains("hidTouchRead(&touch);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("hidCircleRead(&circlePad);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("pointerState.X = touchX;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("pointerState.Y = touchY;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("gamepadState.LeftStickX", sourceCode, StringComparison.Ordinal);
        Assert.Contains("gamepadState.LeftStickY", sourceCode, StringComparison.Ordinal);
        Assert.Contains("KEY_DRIGHT", sourceCode, StringComparison.Ordinal);
        Assert.Contains("KEY_DUP", sourceCode, StringComparison.Ordinal);
        Assert.Contains("KEY_A", sourceCode, StringComparison.Ordinal);
        Assert.Contains("KEY_B", sourceCode, StringComparison.Ordinal);
        Assert.Contains("KEY_X", sourceCode, StringComparison.Ordinal);
        Assert.Contains("KEY_Y", sourceCode, StringComparison.Ordinal);
        Assert.Contains("KEY_ZL", sourceCode, StringComparison.Ordinal);
        Assert.Contains("KEY_ZR", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the New Nintendo 3DS analog C-stick is exposed through the shared right-stick camera axes.
    /// </summary>
    [Fact]
    public void Source_whenBackendCapturesFrames_mapsAnalogCStickToRightStickAxes() {
        string repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        string sourcePath = Path.Combine(repositoryRootPath, "src", "platform", "3ds", "Nintendo3DsStartupInputBackend.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("circlePosition cStick {};", sourceCode, StringComparison.Ordinal);
        Assert.Contains("irrstCstickRead(&cStick);", sourceCode, StringComparison.Ordinal);
        Assert.Contains("gamepadState.RightStickX = static_cast<int16_t>", sourceCode, StringComparison.Ordinal);
        Assert.Contains("gamepadState.RightStickY = static_cast<int16_t>", sourceCode, StringComparison.Ordinal);
    }
}
