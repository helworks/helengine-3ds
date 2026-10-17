global using helengine;
using DemoDisc.menu;

/// <summary>Exercises title lookup against real entity collection and disposal contracts.</summary>
internal static class DemoDiscTitleSearchTests {
    /// <summary>Counts independent regression assertions emitted to the validation log.</summary>
    static int Checks;

    /// <summary>Verifies optional collections without suppressing invalid entities or changing borrowed ownership.</summary>
    public static void Main() {
        Core core = new Core(new CoreInitializationOptions {
            ContentStreamSource = new HostFileSystemContentStreamSource(AppContext.BaseDirectory),
            SceneCatalog = new RuntimeSceneCatalog(new[] { new RuntimeSceneCatalogEntry("menu", "cooked/menu.hasset") })
        });
        core.Initialize(null, null, null, new PlatformInfo("test", "test"));
        Entity root = Node(core);
        Entity leaf = new Entity(core);
        leaf.InitComponents();
        leaf.AddComponent(new RoundedRectComponent());
        root.AddChild(leaf);
        Check(leaf.Children == null, "Debug-overlay-style leaf has no child collection");
        Entity titleBranch = Node(core);
        root.AddChild(titleBranch);
        TextComponent title = new TextComponent { Text = "HELENGINE" };
        titleBranch.AddComponent(title);
        Check(ReferenceEquals(DemoDiscTitleLayout.Find(root, "HELENGINE"), title), "Search reaches title beyond legitimate leaf and returns its borrowed identity");
        Check(DemoDiscTitleLayout.Find(root, "helengine") == null, "Heading matching remains case-sensitive");
        Entity branch = new Entity(core);
        branch.InitChildren();
        Entity nested = Node(core);
        branch.AddChild(nested);
        TextComponent second = new TextComponent { Text = "DEMO DISC" };
        nested.AddComponent(second);
        Check(ReferenceEquals(DemoDiscTitleLayout.Find(branch, "DEMO DISC"), second), "A children-only branch is traversable");
        bool nullRejected = false;
        try { DemoDiscTitleLayout.Find(null, "HELENGINE"); }
        catch (ArgumentNullException) { nullRejected = true; }
        Check(nullRejected, "An invalid null entity throws explicitly");
        root.Children.Add(null);
        bool invalidChildRejected = false;
        try { DemoDiscTitleLayout.Find(root, "missing"); }
        catch (ArgumentNullException) { invalidChildRejected = true; }
        root.Children.RemoveAt(root.Children.Count - 1);
        Check(invalidChildRejected, "An invalid null child is not silently hidden");
        Entity disposed = Node(core);
        disposed.Dispose();
        bool disposedRejected = false;
        try { DemoDiscTitleLayout.Find(disposed, "HELENGINE"); }
        catch (InvalidOperationException) { disposedRejected = true; }
        Check(disposedRejected, "Disposed entities retain their failure behavior");
        Check(title.Parent == titleBranch && second.Parent == nested, "Lookup does not detach or dispose borrowed headings");
        Console.WriteLine($"PASS {Checks} title-search checks");
    }

    /// <summary>Creates an ordinary fully initialized hierarchy node for the regression fixture.</summary>
    static Entity Node(Core core) {
        Entity entity = new Entity(core);
        entity.InitComponents();
        entity.InitChildren();
        return entity;
    }

    /// <summary>Stops validation immediately when a real entity contract is violated.</summary>
    static void Check(bool condition, string message) {
        if (!condition) { throw new InvalidOperationException(message); }
        Checks++;
        Console.WriteLine("PASS " + message);
    }
}
