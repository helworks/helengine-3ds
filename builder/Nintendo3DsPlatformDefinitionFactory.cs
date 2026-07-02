using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;

namespace helengine.nintendo3ds.builder;

/// <summary>
/// Creates the typed Nintendo 3DS platform metadata exposed to the editor.
/// </summary>
public static class Nintendo3DsPlatformDefinitionFactory {
    /// <summary>
    /// Generic native numeric type remaps required by C++ platforms that cannot emit System.Numerics runtime types directly.
    /// </summary>
    const string NativeNumericTypeRemaps = "System.Numerics.Vector2=helengine.float2;System.Numerics.Vector3=helengine.float3;System.Numerics.Vector4=helengine.float4;System.Numerics.Quaternion=helengine.float4";

    /// <summary>
    /// Generic generated-math-convention value that instructs the shared C++ generator to emit native column-vector math helpers.
    /// </summary>
    const string NativeColumnVectorMathConvention = "native-column-vector";

    /// <summary>
    /// Generic pointer-size contract forwarded to the shared C++ generator for Nintendo 3DS native output.
    /// </summary>
    const string PointerSizeInBytes = "4";

    /// <summary>
    /// Generic native file-system header contract forwarded to the shared C++ generator so packaged RomFS access routes through the Nintendo 3DS host bridge.
    /// </summary>
    const string NativeFileSystemHeader = "\"platform/3ds/Nintendo3DsRomFsFileSystem.hpp\"";

    /// <summary>
    /// Generic native file-system type contract forwarded to the shared C++ generator so packaged RomFS access routes through the Nintendo 3DS host bridge.
    /// </summary>
    const string NativeFileSystemType = "helengine::nintendo3ds::Nintendo3DsRomFsFileSystem";

    /// <summary>
    /// Creates the serialized default Nintendo 3DS texture settings contract used when assets do not provide an explicit Nintendo 3DS override.
    /// </summary>
    /// <returns>Serialized default Nintendo 3DS texture settings.</returns>
    static string CreateDefaultSerializedTextureCookSettings() {
        return Nintendo3DsTextureCookSettingsSerializer.Serialize(
            1024,
            TextureAssetColorFormat.Rgba32,
            TextureAssetAlphaPrecision.A8);
    }

    /// <summary>
    /// Creates the serialized default Nintendo 3DS font-atlas texture settings contract used when fonts do not provide an explicit Nintendo 3DS override.
    /// </summary>
    /// <returns>Serialized default Nintendo 3DS font-atlas texture settings.</returns>
    static string CreateDefaultSerializedFontAtlasTextureCookSettings() {
        return Nintendo3DsTextureCookSettingsSerializer.Serialize(
            1024,
            TextureAssetColorFormat.Rgba32,
            TextureAssetAlphaPrecision.A8);
    }

    /// <summary>
    /// Creates the generic texture format capability metadata supported by the first Nintendo 3DS sprite-texture upload contract.
    /// </summary>
    /// <returns>Texture capability metadata for Nintendo 3DS builder-owned texture cook contracts.</returns>
    static PlatformTextureFormatCapabilityDefinition CreateTextureFormatCapabilities() {
        return new PlatformTextureFormatCapabilityDefinition(
            [
                TextureAssetColorFormat.Rgba32.ToString()
            ],
            [
                TextureAssetAlphaPrecision.A8
            ],
            [
                new PlatformTextureFormatCombinationDefinition(TextureAssetColorFormat.Rgba32.ToString(), TextureAssetAlphaPrecision.A8)
            ]);
    }

    /// <summary>
    /// Creates the initial Nintendo 3DS platform definition for the RomFS startup-manifest slice.
    /// </summary>
    /// <returns>The Nintendo 3DS platform definition consumed by the editor.</returns>
    public static PlatformDefinition Create() {
        return new PlatformDefinition(
            "3ds",
            "Nintendo 3DS",
            [
                new PlatformBuildProfileDefinition(
                    "3ds-default",
                    "3DS Default",
                    "Nintendo 3DS startup-manifest packaging build",
                    "3ds-bootstrap",
                    "default",
                    [
                        new PlatformSettingDefinition(
                            "startup-top-screen-color",
                            "Startup Top Screen Color",
                            PlatformSettingKind.Text,
                            "#FF0000",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "startup-bottom-screen-color",
                            "Startup Bottom Screen Color",
                            PlatformSettingKind.Text,
                            "#0000FF",
                            true,
                            [])
                    ])
            ],
            [
                new PlatformGraphicsProfileDefinition(
                    "3ds-bootstrap",
                    "3DS Bootstrap",
                    "Nintendo 3DS bootstrap rendering profile.",
                    [])
            ],
            [],
            [
                new PlatformMaterialSchemaDefinition(
                    Nintendo3DsMaterialSchemaIds.StandardTexturedSchemaId,
                    "3DS Standard Textured",
                    ["3ds-bootstrap"],
                    [
                        new PlatformMaterialFieldDefinition(
                            Nintendo3DsMaterialSchemaIds.TextureRelativePathFieldId,
                            "Texture",
                            PlatformMaterialFieldKind.Text,
                            string.Empty,
                            false,
                            []),
                        new PlatformMaterialFieldDefinition(
                            Nintendo3DsMaterialSchemaIds.DoubleSidedFieldId,
                            "Double Sided",
                            PlatformMaterialFieldKind.Boolean,
                            "false",
                            true,
                            []),
                        new PlatformMaterialFieldDefinition(
                            Nintendo3DsMaterialSchemaIds.VertexColorModeFieldId,
                            "Vertex Color",
                            PlatformMaterialFieldKind.Choice,
                            "multiply",
                            true,
                            ["multiply", "ignore"]),
                        new PlatformMaterialFieldDefinition(
                            Nintendo3DsMaterialSchemaIds.BaseColorFieldId,
                            "Base Color",
                            PlatformMaterialFieldKind.Color,
                            "#FFFFFFFF",
                            true,
                            []),
                        new PlatformMaterialFieldDefinition(
                            Nintendo3DsMaterialSchemaIds.LightingModeFieldId,
                            "Lighting",
                            PlatformMaterialFieldKind.Choice,
                            "lit",
                            true,
                            ["lit", "unlit"])
                    ])
            ],
            [],
            [
                new PlatformCodegenProfileDefinition(
                    "default",
                    "Default",
                    "Nintendo 3DS C# to C++ codegen profile",
                    PlatformCodegenLanguage.Cpp,
                    PlatformSerializationEndianness.LittleEndian,
                    [
                        new PlatformSettingDefinition(
                            "write-conversion-report",
                            "Write Conversion Report",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "include-project-defined-preprocessor-symbols",
                            "Include Project Symbols",
                            PlatformSettingKind.Boolean,
                            "false",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "load-native-runtime-metadata",
                            "Load Native Runtime Metadata",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "generated-math-convention",
                            "Generated Math Convention",
                            PlatformSettingKind.Text,
                            NativeColumnVectorMathConvention,
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "pointer-size-bytes",
                            "Pointer Size (Bytes)",
                            PlatformSettingKind.Text,
                            PointerSizeInBytes,
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "type-remaps",
                            "Type Remaps",
                            PlatformSettingKind.Text,
                            NativeNumericTypeRemaps,
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "native-file-system-header",
                            "Native File System Header",
                            PlatformSettingKind.Text,
                            NativeFileSystemHeader,
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "native-file-system-type",
                            "Native File System Type",
                            PlatformSettingKind.Text,
                            NativeFileSystemType,
                            true,
                            [])
                    ])
            ],
            [
                new PlatformStorageProfileDefinition(
                    "romfs-package",
                    "RomFS Package",
                    PlatformStorageProfileKind.LooseFiles,
                    "3ds-romfs-package",
                    allowContainerSegmentation: false)
            ],
            [
                new PlatformMediaProfileDefinition(
                    "3dsx-homebrew",
                    "3DSX Homebrew",
                    PlatformMediaLayoutKind.InstallTree,
                    allowPhysicalDuplication: false,
                    preferLocalityOverDeduplication: true)
            ],
            new RuntimeGenerationContract(
                RuntimeMaterialResolutionMode.CookedPlatformOwned,
                true,
                PackagedPathPolicy.ContentRelativeOnly,
                []),
            assetCookCapabilities: [
                new PlatformAssetCookCapabilityDefinition(
                    "texture",
                    "runtime-texture",
                    PlatformAssetCookOwnershipKind.BuilderOwned,
                    "3ds-texture",
                    CreateDefaultSerializedTextureCookSettings(),
                    CreateTextureFormatCapabilities()),
                new PlatformAssetCookCapabilityDefinition(
                    "font-atlas-texture",
                    "runtime-texture",
                    PlatformAssetCookOwnershipKind.BuilderOwned,
                    "3ds-font-atlas-texture",
                    CreateDefaultSerializedFontAtlasTextureCookSettings(),
                    CreateTextureFormatCapabilities())
            ]);
    }
}
