/* Read only an already-loaded support prefab; never instantiate/load/set height. */
static void *preview_support_prefab(void *station, PFRoots *roots, void **outRoot) {
    void *definition = pf_invoke(get_station_method(p_object_get_class(station), "get_Definition",
                                                    "TPS.Game.GameItemDefinition"),
                                 station, 0);
    if (!definition || !pf_root(roots, definition)) {
        placement_log("SUPPORT LOOKUP: station definition unavailable");
        return 0;
    }
    void *base = findclass(p_domain_get(), "TPS.Game", "EntityDefinition");
    void *components =
        base ? pf_ref(definition, base, "Components", "TPS.Game.IComponentProxy[]") : 0;
    int componentCount = 0, rideType = 0, found = 0;
    if (!components || !pf_root(roots, components) ||
        !write_array_count(components, &componentCount)) {
        placement_log("SUPPORT LOOKUP: station proxies unavailable");
        return 0;
    }
    for (int i = 0; i < componentCount; ++i) {
        void *proxy = write_array_item(components, i);
        if (!proxy || !pf_class(p_object_get_class(proxy), "TPS.Game.ECPTrackRide"))
            continue;
        void *field =
            pf_field(p_object_get_class(proxy), "trackRideType", "TPS.Game.TrackRideType");
        if (!field || !write_value_layout(p_class_from_type(p_field_get_type(field)), 4))
            return 0;
        p_field_get_value(proxy, field, &rideType);
        ++found;
    }
    if (found != 1) {
        placement_log("SUPPORT LOOKUP: exact coaster type unavailable");
        return 0;
    }
    void *resources = findclass(p_domain_get(), "UnityEngine", "Resources"),
         *globalClass = findclass(p_domain_get(), "TPS.Game", "TrackRidesConfig");
    if (!globalClass)
        return 0;
    void *typeObject = p_type_get_object(p_class_get_type(globalClass));
    const char *tp[] = {"System.Type"};
    void *ta[] = {typeObject};
    void *all = pf_invoke(
        pf_method(resources, "FindObjectsOfTypeAll", "UnityEngine.Object[]", 1, tp, 0), 0, ta);
    int total = 0;
    if (!all || !pf_root(roots, all) || !write_array_count(all, &total) || total != 1) {
        placement_log("SUPPORT LOOKUP: unique global ride config unavailable");
        return 0;
    }
    void *global = write_array_item(all, 0);
    const char *rp[] = {"TPS.Game.TrackRideType"};
    void *ra[] = {&rideType};
    void *config = pf_invoke(
        pf_method(globalClass, "GetTrackRideConfig", "TPS.Game.TrackRideConfig", 1, rp, 1), global,
        ra);
    if (!config || !pf_root(roots, config)) {
        placement_log("SUPPORT LOOKUP: coaster config unavailable");
        return 0;
    }
    void *f =
        pf_field(p_object_get_class(config), "SupportDefinition", "TPS.Game.GameItemDefinitionID");
    if (!f || !write_value_layout(p_class_from_type(p_field_get_type(f)), 8))
        return 0;
    unsigned char id[8];
    p_field_get_value(config, f, id);
    void *db = findclass(p_domain_get(), "TPS.Game", "LevelDatabaseUtils");
    const char *dp[] = {"TPS.Game.GameItemDefinitionID"};
    void *da[] = {id};
    void *d = pf_invoke(pf_method(db, "Get", "TPS.Game.GameItemDefinition", 1, dp, 0), 0, da);
    if (!d || !pf_root(roots, d)) {
        placement_log("SUPPORT LOOKUP: support definition unavailable");
        return 0;
    }
    /* Prefer a live, exactly matching support visual; its height attachment table
    remains available even if the addressable reference has released its asset. */
    long long expectedId = 0;
    if (!write_definition_id(d, &expectedId))
        return 0;
    void *supportClass = findclass(p_domain_get(), "TPS.Game", "TrackSupportVisualComponent"),
         *componentClass = findclass(p_domain_get(), "UnityEngine", "Component"),
         *visualClass = findclass(p_domain_get(), "TPS.Game", "GameItemVisual");
    void *supportType = supportClass ? p_type_get_object(p_class_get_type(supportClass)) : 0;
    void *visualType = visualClass ? p_type_get_object(p_class_get_type(visualClass)) : 0;
    void *sa[] = {supportType};
    void *supports = supportType ? pf_invoke(pf_method(resources, "FindObjectsOfTypeAll",
                                                       "UnityEngine.Object[]", 1, tp, 0),
                                             0, sa)
                                 : 0;
    int supportCount = 0;
    if (supports && pf_root(roots, supports) && write_array_count(supports, &supportCount))
        for (int i = 0; i < supportCount; ++i) {
            void *support = write_array_item(supports, i);
            if (!support)
                continue;
            const char *parentParams[] = {"System.Type", "System.Boolean"};
            unsigned char includeInactive = 1;
            void *parentArgs[] = {visualType, &includeInactive};
            void *visual = visualType
                               ? pf_invoke(pf_method(componentClass, "GetComponentInParent",
                                                     "UnityEngine.Component", 2, parentParams, 1),
                                           support, parentArgs)
                               : 0;
            void *item =
                visual ? pf_ref(visual, visualClass, "Item", "TPS.Game.GameItemInstance") : 0;
            void *itemDefinition =
                preview_get(item, "get_Definition", "TPS.Game.GameItemDefinition");
            long long candidateId = 0;
            if (!itemDefinition || !write_definition_id(itemDefinition, &candidateId) ||
                candidateId != expectedId)
                continue;
            void *transform = preview_get(visual, "get_transform", "UnityEngine.Transform");
            if (!transform || !pf_root(roots, support) || !pf_root(roots, transform))
                return 0;
            *outRoot = transform;
            placement_log("SUPPORT LOOKUP: matching existing support visual selected");
            return support;
        }
    void *ref = pf_invoke(
        get_station_method(p_object_get_class(d), "get_VisualsPrefab",
                           "UnityEngine.AddressableAssets.AssetReferenceT<UnityEngine.GameObject>"),
        d, 0);
    if (!ref || !pf_root(roots, ref)) {
        placement_log("SUPPORT LOOKUP: prefab reference accessor unavailable");
        return 0;
    }
    void *asset = pf_invoke(
        get_station_method(p_object_get_class(ref), "get_Asset", "UnityEngine.Object"), ref, 0);
    if (!asset || !pf_class(p_object_get_class(asset), "UnityEngine.GameObject") ||
        !pf_root(roots, asset)) {
        placement_log("SUPPORT LOOKUP: prefab asset is not already loaded");
        return 0;
    }
    void *gc = findclass(p_domain_get(), "UnityEngine", "GameObject"),
         *sc = findclass(p_domain_get(), "TPS.Game", "TrackSupportVisualComponent");
    void *type = sc ? p_type_get_object(p_class_get_type(sc)) : 0;
    const char *cp[] = {"System.Type", "System.Boolean"};
    unsigned char yes = 1;
    void *ca[] = {type, &yes};
    void *component =
        type ? pf_invoke(pf_method(gc, "GetComponentInChildren", "UnityEngine.Component", 2, cp, 1),
                         asset, ca)
             : 0;
    void *root =
        pf_invoke(get_station_method(gc, "get_transform", "UnityEngine.Transform"), asset, 0);
    if (!component || !root || !pf_root(roots, component) || !pf_root(roots, root)) {
        placement_log("SUPPORT LOOKUP: prefab component/root unavailable");
        return 0;
    }
    *outRoot = root;
    return component;
}
static int preview_native_support_top(void *component, void *root, int index, float *out) {
    const char *hp[] = {"System.Int32"};
    void *ha[] = {&index};
    void *point = pf_invoke(pf_method(p_object_get_class(component), "GetSplinePoint",
                                      "UnityEngine.Transform", 1, hp, 1),
                            component, ha);
    if (!point)
        return 0;
    void *position = preview_get(point, "get_position", "UnityEngine.Vector3");
    if (!position || !pf_class(p_object_get_class(position), "UnityEngine.Vector3") ||
        !write_value_layout(p_object_get_class(position), 12))
        return 0;
    float world[3];
    memcpy(world, p_object_unbox(position), 12);
    const char *vp[] = {"UnityEngine.Vector3"};
    void *va[] = {world};
    void *local = pf_invoke(pf_method(p_object_get_class(root), "InverseTransformPoint",
                                      "UnityEngine.Vector3", 1, vp, 1),
                            root, va);
    if (!local || !pf_class(p_object_get_class(local), "UnityEngine.Vector3") ||
        !write_value_layout(p_object_get_class(local), 12))
        return 0;
    memcpy(out, p_object_unbox(local), 12);
    return finite_vector(out, 3) && out[1] > 0 && out[1] < 4096;
}
