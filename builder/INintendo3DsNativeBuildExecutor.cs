namespace helengine.nintendo3ds.builder;

/// <summary>
/// Defines the native Nintendo 3DS build executor seam used by the managed builder.
/// </summary>
public interface INintendo3DsNativeBuildExecutor {
    /// <summary>
    /// Builds one native Nintendo 3DS package for the provided workspace.
    /// </summary>
    /// <param name="workspace">Resolved build workspace to package.</param>
    /// <param name="cancellationToken">Cancellation token used to stop the build cooperatively.</param>
    void Build(Nintendo3DsBuildWorkspace workspace, CancellationToken cancellationToken);
}
