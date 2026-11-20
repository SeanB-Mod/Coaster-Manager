/* Library Build uses normal Funfair placement, then imports only into its retained item. */
static native_ui_lateupdate placement_original;
static volatile long placement_request, placement_callbacks;
static unsigned placement_thread;
static int placement_attempted, placement_import_pending;
static long long placement_expected_definition;
static PFRoots placement_wait_roots;
static void *placement_wait_item;
static unsigned long long placement_wait_started, placement_cursor_ended, placement_next_check;
static int placement_support_cleared, placement_finish_attempted;
static UINT_PTR placement_queue_diagnostic;
static void footprint_clear(void);
static void placement_wait_clear(void) {
    footprint_clear();
    placement_wait_item = 0;
    placement_wait_started = placement_cursor_ended = 0;
    placement_next_check = 0;
    placement_support_cleared = 0;
    placement_finish_attempted = 0;
    write_selection_frozen = 0;
    pf_release(&placement_wait_roots);
}
static void placement_log(const char *s) {
    logtext("STATION PLACEMENT: ");
    logtext(s);
    nl();
    FlushFileBuffers(g_log);
}
#include "placement_footprint.inc"
static int placement_hide_station_arrows(void *context) {
    PFRoots roots = {0};
    int ok = 0;
    if (!context)
        return 0;
    void *visual = pf_ref(context, p_object_get_class(context), "trackRideVisualComponent",
                          "TPS.Game.TrackRideVisualComponent");
    if (!visual || !pf_class(p_object_get_class(visual), "TPS.Game.TrackRideVisualComponent") ||
        !pf_root(&roots, visual))
        goto done;
    const char *params[] = {"System.Boolean"};
    void *c = p_object_get_class(visual);
    void *entry = pf_method(c, "ToggleEntryArrow", "System.Void", 1, params, 1);
    void *exit = pf_method(c, "ToggleExitArrow", "System.Void", 1, params, 1);
    if (!entry || !exit ||
        !write_value_layout(p_class_from_type(p_method_get_param(entry, 0)), 1) ||
        !write_value_layout(p_class_from_type(p_method_get_param(exit, 0)), 1) ||
        !write_code_guard(entry, 0x2ed5d90, 0x20, 0xd775f8e0U) ||
        !write_code_guard(exit, 0x2ed5db0, 0x20, 0xc48ff079U))
        goto done;
    unsigned char hidden = 0;
    void *args[] = {&hidden};
    ok = write_void(entry, visual, args) && write_void(exit, visual, args);
done:
    pf_release(&roots);
    return ok;
}
static void placement_import_finished(int verified, void *context) {
    if (!placement_import_pending)
        return;
    placement_import_pending = 0;
    if (!verified) {
        placement_log(
            "Automatic import outcome unverified; return to selection mode to retry with a fresh station");
        return;
    }
    placement_log(
        placement_hide_station_arrows(context)
            ? "STATION VISUAL CLEANUP: entry/exit arrows hidden"
            : "STATION VISUAL CLEANUP unavailable; track verification passed; inspect marker");
    placement_attempted = 0;
    placement_log(
        "F10 READY: another template placement allowed after returning to normal selection mode");
}
static void *placement_singleton(const char *name, const char *full, const char *parentName) {
    void *c = findclass(p_domain_get(), "TPS.Game", name);
    void *parent = c ? p_class_get_parent(c) : 0;
    if (!parent || !pf_class(parent, parentName))
        return 0;
    void *f = p_class_get_field_from_name(parent, "<Instance>k__BackingField");
    void *o = 0;
    if (!f || p_field_get_parent(f) != parent || !(p_field_get_flags(f) & 0x10) ||
        !pf_type(p_field_get_type(f), full))
        return 0;
    p_field_static_get_value(f, &o);
    return o && p_object_get_class(o) == c ? o : 0;
}
static int placement_boolean(void *o, void *c, const char *name, int wanted) {
    void *f = pf_field(c, name, "System.Boolean");
    if (!f || !write_value_layout(p_class_from_type(p_field_get_type(f)), 1))
        return 0;
    unsigned char value = 2;
    p_field_get_value(o, f, &value);
    return value == wanted;
}
static int placement_has_track_proxy(void *definition, PFRoots *roots) {
    void *c = findclass(p_domain_get(), "TPS.Game", "EntityDefinition");
    if (!c || !p_class_is_assignable_from(c, p_object_get_class(definition)))
        return 0;
    void *array = pf_ref(definition, c, "Components", "TPS.Game.IComponentProxy[]");
    int count = 0;
    if (!array || !pf_root(roots, array) || !write_array_count(array, &count))
        return 0;
    unsigned matches = 0;
    for (int i = 0; i < count; ++i) {
        PFRoots temp = {0};
        void *proxy = write_array_item(array, i);
        if (!proxy || !pf_root(&temp, proxy)) {
            pf_release(&temp);
            return 0;
        }
        if (pf_class(p_object_get_class(proxy), "TPS.Game.ECPTrackRide"))
            ++matches;
        pf_release(&temp);
    }
    return matches == 1;
}
static int funfair_property_true(void *o, const char *name) {
    void *m = get_station_method(p_object_get_class(o), name, "System.Boolean");
    void *b = pf_invoke(m, o, 0);
    if (!b || !pf_class(p_object_get_class(b), "System.Boolean") ||
        !write_value_layout(p_object_get_class(b), 1))
        return 0;
    void *v = p_object_unbox(b);
    return v && *(unsigned char *)v == 1;
}
static void *funfair_menu(PFRoots *roots) {
    void *c = findclass(p_domain_get(), "TPS.Game.UI", "HUDManager");
    void *parent = c ? p_class_get_parent(c) : 0;
    if (!parent || !pf_class(parent, "TPS.Core.StaticGameSystem<TPS.Game.UI.HUDManager>"))
        return 0;
    void *f = p_class_get_field_from_name(parent, "<Instance>k__BackingField");
    void *h = 0;
    if (!f || p_field_get_parent(f) != parent || !(p_field_get_flags(f) & 0x10) ||
        !pf_type(p_field_get_type(f), "TPS.Game.UI.HUDManager"))
        return 0;
    p_field_static_get_value(f, &h);
    if (!h || p_object_get_class(h) != c || !pf_root(roots, h))
        return 0;
    const char *params[] = {"System.Type"};
    void *m = pf_method(c, "GetElement", "TPS.Game.UI.IHUDElement", 1, params, 1);
    void *menuClass = findclass(p_domain_get(), "TPS.Game.UI", "RideBuildingMenu");
    void *t = menuClass ? p_type_get_object(p_class_get_type(menuClass)) : 0;
    if (!t || !pf_root(roots, t))
        return 0;
    void *args[] = {t};
    void *menu = pf_invoke(m, h, args);
    return menu && p_object_get_class(menu) == menuClass && pf_root(roots, menu) &&
                   funfair_property_true(menu, "get_isActiveAndEnabled") &&
                   funfair_property_true(menu, "get_IsOpen")
               ? menu
               : 0;
}
/* Resolve the live row binding; never manufacture UI data or copy a stale row. */
static int funfair_row_matches(void *row, void *menu, long long expected, PFRoots *roots) {
    void *c = p_object_get_class(row);
    if (!pf_class(c, "TPS.Game.UI.RideBuildingMenu_Item") ||
        !funfair_property_true(row, "get_isActiveAndEnabled"))
        return 0;
    if (pf_ref(row, c, "_parentMenu", "TPS.Game.UI.RideBuildingMenu") != menu)
        return 0;
    void *data = pf_ref(row, c, "_currentData", "TPS.Game.UI.RideBuildingMenu.ItemData");
    if (!data || !pf_root(roots, data))
        return 0;
    void *available = pf_ref(data, p_object_get_class(data), "AvailalableExhibitRideBlueprint",
                             "TPS.Game.AvailableExhibitRideBlueprint");
    if (!available || !pf_root(roots, available))
        return 0;
    void *get = get_station_method(p_object_get_class(available), "get_Definition",
                                   "TPS.Game.ExhibitRideBlueprintDefinition");
    void *blueprint = pf_invoke(get, available, 0);
    if (!blueprint ||
        !pf_class(p_object_get_class(blueprint), "TPS.Game.ExhibitRideBlueprintDefinition") ||
        !pf_root(roots, blueprint))
        return 0;
    get = get_station_method(p_object_get_class(blueprint), "get_ItemDefinition",
                             "TPS.Game.GameItemDefinition");
    void *item = pf_invoke(get, blueprint, 0);
    long long id = 0;
    void *dc = findclass(p_domain_get(), "TPS.Game", "GameItemDefinition");
    if (!item || !dc || !p_class_is_assignable_from(dc, p_object_get_class(item)) ||
        !pf_root(roots, item) || !write_definition_id(item, &id) || id != expected)
        return 0;
    int required = 0, collected = 0;
    if (!write_read_limit(blueprint, p_object_get_class(blueprint), "NumRequiredPieces",
                          "System.Int32", &required, 4) ||
        required < 0 ||
        !write_read_limit(available, p_object_get_class(available), "PiecesCollected",
                          "System.Int32", &collected, 4) ||
        required != collected)
        return 0;
    return 1;
}
static int funfair_can_buy(void *row) {
    void *c = p_object_get_class(row);
    void *data = pf_ref(row, c, "_currentData", "TPS.Game.UI.RideBuildingMenu.ItemData");
    void *button = pf_ref(row, c, "_buttonBuyRide", "TPS.Core.UI.UIButton");
    int cost = 0;
    return data && button && placement_boolean(data, p_object_get_class(data), "CanAfford", 1) &&
           write_read_limit(data, p_object_get_class(data), "BuildCost", "System.Int32", &cost,
                            4) &&
           cost > 0 && funfair_property_true(button, "get_isActiveAndEnabled") &&
           funfair_property_true(button, "get_interactable");
}
static int placement_dispatch(void *m, void *row) {
    if (!m || !row || placement_attempted)
        return 0;
    placement_attempted = 1;
    placement_log(
        "dispatching live native purchase-button PrimaryPress; normal purchase cost/quality/upgrades; automatic import waits for the exact placed station; no automatic save");
    return write_void(m, row, 0);
}
/* Use the game's live UI action instead of pinning a private purchase body. */
static int funfair_press_live_button(void *row, PFRoots *roots) {
    if (!row || !funfair_can_buy(row)) {
        placement_log("SKIP: native purchase button no longer usable");
        return 0;
    }
    void *button = pf_ref(row, p_object_get_class(row), "_buttonBuyRide", "TPS.Core.UI.UIButton");
    if (!button || !pf_class(p_object_get_class(button), "TPS.Core.UI.UIButton") ||
        !pf_root(roots, button)) {
        placement_log("SKIP: live native purchase button type or lifetime differs");
        return 0;
    }
    void *press = pf_method(p_object_get_class(button), "PrimaryPress", "System.Void", 0, 0, 1);
    if (!press) {
        placement_log("SKIP: native purchase action signature unavailable or ambiguous");
        return 0;
    }
    return placement_dispatch(press, button);
}
static int funfair_purchase(long long stationId, PFRoots *roots) {
    void *menu = funfair_menu(roots);
    if (!menu) {
        placement_log("SKIP: open Funfair Templates and show the matching coaster row before F10");
        return 0;
    }
    void *get =
        get_station_method(p_object_get_class(menu), "get_gameObject", "UnityEngine.GameObject");
    void *gameObject = pf_invoke(get, menu, 0);
    if (!gameObject || !pf_root(roots, gameObject))
        return 0;
    void *gc = findclass(p_domain_get(), "UnityEngine", "GameObject");
    const char *params[] = {"System.Type", "System.Boolean"};
    void *m =
        gc ? pf_method(gc, "GetComponentsInChildren", "UnityEngine.Component[]", 2, params, 1) : 0;
    void *rowClass = findclass(p_domain_get(), "TPS.Game.UI", "RideBuildingMenu_Item");
    void *t = rowClass ? p_type_get_object(p_class_get_type(rowClass)) : 0;
    if (!t || !pf_root(roots, t))
        return 0;
    unsigned char includeInactive = 0;
    void *args[] = {t, &includeInactive};
    void *array = pf_invoke(m, gameObject, args);
    int count = 0;
    if (!array || !pf_root(roots, array) || !write_array_count(array, &count))
        return 0;
    unsigned matches = 0;
    void *chosen = 0;
    for (int i = 0; i < count; ++i) {
        PFRoots temp = {0};
        void *row = write_array_item(array, i);
        if (!row || !pf_root(&temp, row)) {
            pf_release(&temp);
            return 0;
        }
        if (funfair_row_matches(row, menu, stationId, &temp)) {
            ++matches;
            if (!chosen) {
                chosen = row;
                if (!pf_root(roots, chosen)) {
                    pf_release(&temp);
                    return 0;
                }
            }
        }
        pf_release(&temp);
    }
    if (matches != 1 || !chosen) {
        placement_log(
            "SKIP: require exactly one visible, unlocked Funfair row matching the selected station definition; clear search/show Roller Coasters");
        return 0;
    }
    PFRoots check = {0};
    int stable = funfair_row_matches(chosen, menu, stationId, &check) && funfair_can_buy(chosen);
    pf_release(&check);
    if (!stable) {
        placement_log("SKIP: matching ride purchase button is disabled, unaffordable or changed");
        return 0;
    }
    verify_support_orientation();
    if(!g_support_world_verified){placement_log("SKIP: snapshot world/local orientation check failed before purchase");return 0;}
    return funfair_press_live_button(chosen, roots);
}
/* Explicit fresh placement abandons only our old wait; never replays a failed event. */
static int placement_reset_failed(int normalCursor, int queuesIdle, int commit) {
    if (write_pending || placement_import_pending || !normalCursor || !queuesIdle)
        return 0;
    if (commit) {
        placement_wait_clear();
        placement_attempted = 0;
        write_attempted = 0;
        write_release_retry();
        placement_log(
            "RETRY READY: previous attempt released; next request creates a fresh station; previous coaster unchanged");
    }
    return 1;
}
static int placement_prepare_retry(int commit) {
    if (write_pending || placement_import_pending)
        return 0;
    if (!placement_attempted && !write_attempted && !placement_wait_item)
        return 1;
    if (!p_thread_current() || !write_retry_idle())
        return 0;
    PFRoots roots = {0};
    int ready = 0;
    void *cursor = placement_singleton("CursorManager", "TPS.Game.CursorManager",
                                       "TPS.Core.StaticGameSystem<TPS.Game.CursorManager>");
    if (!cursor || !pf_root(&roots, cursor) ||
        !placement_boolean(cursor, p_object_get_class(cursor), "_hasOngoingModeChange", 0))
        goto done;
    void *field = pf_field(p_object_get_class(cursor), "_mode", "TPS.Game.CursorMode");
    void *mode = 0;
    if (!field)
        goto done;
    p_field_get_value(cursor, field, &mode);
    if (!mode || !pf_root(&roots, mode) ||
        !pf_class(p_object_get_class(mode), "TPS.Game.CursorModeSelect"))
        goto done;
    void *bus = write_event_bus(&roots);
    int total = 0, active = 0;
    if (!bus || !write_queue_state(bus, 0, &total, &active, 0) || total || active)
        goto done;
    ready = placement_reset_failed(1, 1, commit);
done:
    pf_release(&roots);
    return ready;
}
static void placement_report(void) {
    if (!write_library_selection)
        footprint_curve_count = 0;
    logtext("\r\n---- COASTER TEMPLATE PLACEMENT ----\r\n");
    PFRoots roots = {0};
    if (!p_thread_current()) {
        placement_log("SKIP: no IL2CPP thread context");
        return;
    }
    if (!placement_prepare_retry(1)) {
        placement_log(
            "SKIP: finish/cancel active placement and wait for queued events before retrying");
        return;
    }
    if (!write_load_selection() || !write_station_definition_id) {
        placement_log(
            "SKIP: F10 requires a v0.5.7 or newer F8 export prepared with the new helper (TPMPLAN3)");
        return;
    }
    if (placement_expected_definition &&
        write_station_definition_id != placement_expected_definition) {
        placement_log("SKIP: browser coaster type differs from current selection");
        return;
    }
    void *manager = placement_singleton("LevelManager", "TPS.Game.LevelManager",
                                        "TPS.Core.StaticGameSystem<TPS.Game.LevelManager>");
    void *cursor = placement_singleton("CursorManager", "TPS.Game.CursorManager",
                                       "TPS.Core.StaticGameSystem<TPS.Game.CursorManager>");
    if (!manager || !cursor || !pf_root(&roots, manager) || !pf_root(&roots, cursor)) {
        placement_log("SKIP: active level/cursor systems unavailable");
        goto done;
    }
    void *level = pf_ref(manager, p_object_get_class(manager), "_level", "TPS.Game.Level");
    if (!level || !pf_root(&roots, level) ||
        !placement_boolean(level, p_object_get_class(level), "<LoadHUDComplete>k__BackingField",
                           1) ||
        !placement_boolean(level, p_object_get_class(level), "IsLoadingScenario", 0) ||
        !placement_boolean(level, p_object_get_class(level), "<IsVisible>k__BackingField", 1)) {
        placement_log("SKIP: museum is loading or not visible");
        goto done;
    }
    void *cursorClass = p_object_get_class(cursor);
    void *modeField = pf_field(cursorClass, "_mode", "TPS.Game.CursorMode");
    void *mode = 0;
    if (!modeField || !placement_boolean(cursor, cursorClass, "_hasOngoingModeChange", 0)) {
        placement_log("SKIP: cursor mode layout or transition state unavailable");
        goto done;
    }
    p_field_get_value(cursor, modeField, &mode);
    if (!mode || !pf_root(&roots, mode) ||
        !pf_class(p_object_get_class(mode), "TPS.Game.CursorModeSelect")) {
        placement_log("SKIP: return to normal selection mode; close build/coaster controls first");
        goto done;
    }
    void *db = findclass(p_domain_get(), "TPS.Game", "LevelDatabaseUtils");
    const char *idp[] = {"TPS.Game.GameItemDefinitionID"};
    void *resolve = db ? pf_method(db, "Get", "TPS.Game.GameItemDefinition", 1, idp, 0) : 0;
    if (!resolve || !write_value_layout(p_class_from_type(p_method_get_param(resolve, 0)), 8)) {
        placement_log("SKIP: station definition resolver unavailable");
        goto done;
    }
    void *da[] = {&write_station_definition_id};
    void *definition = pf_invoke(resolve, 0, da);
    long long actual = 0;
    void *dc = findclass(p_domain_get(), "TPS.Game", "GameItemDefinition");
    if (!definition || !dc || !p_class_is_assignable_from(dc, p_object_get_class(definition)) ||
        !pf_root(&roots, definition) || !write_definition_id(definition, &actual) ||
        actual != write_station_definition_id || !placement_has_track_proxy(definition, &roots)) {
        placement_log(
            "SKIP: station definition unresolved, mismatched or not a single track-ride definition");
        goto done;
    }
    if (!funfair_purchase(write_station_definition_id, &roots)) {
        if (placement_attempted)
            placement_log(
                "UNVERIFIED: Funfair purchase raised an exception; inspect the coaster, cancel placement and return to selection mode before retrying");
        goto done;
    }
    void *c = findclass(p_domain_get(), "TPS.Game", "BuildLogic");
    void *get =
        c ? pf_method(c, "GetCursorModePlaceItem", "TPS.Game.CursorModePlaceItem", 0, 0, 0) : 0;
    void *placedCursor = pf_invoke(get, 0, 0);
    if (!placedCursor || !pf_root(&roots, placedCursor) ||
        !pf_class(p_object_get_class(placedCursor), "TPS.Game.CursorModePlaceItem")) {
        placement_log("UNVERIFIED: placement returned without an active item-placement cursor");
        goto done;
    }
    void *itemGetter = get_station_method(p_object_get_class(placedCursor), "get_Item",
                                          "TPS.Game.GameItemInstance");
    void *item = pf_invoke(itemGetter, placedCursor, 0);
    long long itemDefinition = 0;
    if (!item || !g_gameitem_class ||
        !p_class_is_assignable_from(g_gameitem_class, p_object_get_class(item)) ||
        !pf_root(&placement_wait_roots, item) ||
        (itemDefinition = export_station_definition_id(item)) != write_station_definition_id) {
        placement_wait_clear();
        placement_log("AUTO IMPORT DISABLED: could not retain exact placement item");
        goto done;
    }
    placement_wait_item = item;
    placement_wait_started = GetTickCount64();
    write_selection_frozen = 1;
    footprint_start(item);

    placement_log(
        "FUNFAIR CURSOR ACTIVE: position/rotate and click to place. Automatic import is armed for this exact station and frozen template; cancel normally to abort");
done:
    pf_release(&roots);
    placement_log("PLACEMENT REQUEST END; automatic import status follows after placement");
}
/* Hide only the support being cleared, through the game's existing visual helper. */
static int placement_hide_arrow(void *m, const unsigned entity[2]) {
    if (!m || !entity || !entity[0] || !entity[1])
        return 0;
    unsigned char hidden = 0;
    void *args[] = {(void *)entity, &hidden};
    return write_void(m, 0, args);
}
static int placement_clear_support(void *self, void *support, void *clear) {
    PFRoots roots = {0};
    int ok = 0;
    if (!support || !g_gameitem_class || !g_entity_field ||
        !p_class_is_assignable_from(g_gameitem_class, p_object_get_class(support)) ||
        !pf_root(&roots, support))
        goto done;
    void *c = findclass(p_domain_get(), "TPS.Game", "ECPTrackRide");
    const char *params[] = {"Unity.Entities.Entity", "System.Boolean"};
    void *m = c ? pf_method(c, "<SetComponents>g__ToggleTrackSupportArrow|20_26", "System.Void", 2,
                            params, 0)
                : 0;
    if (!m || !write_value_layout(p_class_from_type(p_method_get_param(m, 0)), 8) ||
        !write_value_layout(p_class_from_type(p_method_get_param(m, 1)), 1) ||
        !write_code_guard(m, 0x2ea1e00, 0xf0, 0xbaab1121U))
        goto done;
    unsigned entity[2] = {0};
    p_field_get_value(support, g_entity_field, entity);
    if (!placement_hide_arrow(m, entity))
        goto done;
    ok = write_void(clear, self, 0);
    if (ok)
        placement_log("SUPPORT VISUAL CLEANUP: hid retained support arrow before ClearSupport");
done:
    pf_release(&roots);
    return ok;
}
/* Finish only after the native placement history confirms our exact station.
 * GetCursorModePlaceItem can also return a mode beneath another tool: require
 * that it is the current mode, not merely present in the stack. */
static void placement_finish_confirmed(void *self, void *activeCursor) {
    if (placement_finish_attempted || !activeCursor || !self)
        return;
    PFRoots roots = {0};
    void *station = 0, *support = 0;
    p_field_get_value(self, g_station_field, &station);
    p_field_get_value(self, g_support_field, &support);
    if (station != placement_wait_item)
        return;
    void *cursor = placement_singleton("CursorManager", "TPS.Game.CursorManager",
                                       "TPS.Core.StaticGameSystem<TPS.Game.CursorManager>");
    if (!cursor || !pf_root(&roots, cursor) ||
        !placement_boolean(cursor, p_object_get_class(cursor), "_hasOngoingModeChange", 0))
        goto done;
    void *modeField = pf_field(p_object_get_class(cursor), "_mode", "TPS.Game.CursorMode");
    void *mode = 0;
    if (!modeField)
        goto done;
    p_field_get_value(cursor, modeField, &mode);
    if (mode != activeCursor)
        goto done;
    void *get = get_station_method(p_object_get_class(activeCursor), "get_Item",
                                   "TPS.Game.GameItemInstance");
    void *item = get ? pf_invoke(get, activeCursor, 0) : 0;
    if (!item || (item != station && item != support))
        goto done;
    void *historyField = pf_field(p_object_get_class(cursor), "LastItemPlacementData",
                                  "TPS.Game.LastItemPlacementData");
    void *history = historyField ? p_field_get_value_object(historyField, cursor) : 0;
    if (!history || !pf_root(&roots, history))
        goto done;
    void *placed =
        pf_ref(history, p_object_get_class(history), "Item", "TPS.Game.GameItemInstance");
    if (placed != station)
        goto done;
    void *c = findclass(p_domain_get(), "TPS.Game", "BuildLogic");
    void *finish = c ? pf_method(c, "FinishPlaceItem", "System.Void", 0, 0, 0) : 0;
    if (!write_code_guard(finish, 0x130a40b0, 0x1430, 0x8ecf1a69U)) {
        placement_finish_attempted = 1;
        placement_log(
            "AUTO FINISH unavailable: native method guard differs; right-click fallback retained");
        goto done;
    }
    placement_finish_attempted = 1;
    placement_log(
        write_void(finish, 0, 0)
            ? "AUTO FINISH: native placement history confirmed station; requested native tool completion"
            : "AUTO FINISH failed: no retry; right-click fallback retained");
done:
    pf_release(&roots);
}
/* Called only by the existing game-owned coaster UI callback. A cancelled
 * placement cannot pass the live-entity and exact target-binding importer guards. */
static void placement_service_ui(void *self) {
    if (!placement_wait_item || !self || !p_thread_current())
        return;
    unsigned long long now = GetTickCount64();
    if (now - placement_wait_started > 180000) {
        placement_log("AUTO IMPORT TIMEOUT: no verified target became ready within three minutes");
        placement_wait_clear();
        return;
    }
    void *bc = findclass(p_domain_get(), "TPS.Game", "BuildLogic");
    void *get =
        bc ? pf_method(bc, "GetCursorModePlaceItem", "TPS.Game.CursorModePlaceItem", 0, 0, 0) : 0;
    if (!get)
        return;
    void *cursorException = 0;
    void *activeCursor = p_runtime_invoke(get, 0, 0, &cursorException);
    if (cursorException)
        return;
    if (activeCursor) {
        if (now >= placement_next_check) {
            placement_next_check = now + 250;
            if (g_station_field && g_support_field)
                placement_finish_confirmed(self, activeCursor);
        }
        placement_cursor_ended = 0;
        return;
    }
    if (!placement_cursor_ended) {
        placement_cursor_ended = now;
        return;
    }
    if (now - placement_cursor_ended < 250)
        return;
    void *station = 0;
    void *support = 0;
    if (!g_station_field || !g_support_field || !g_entity_field || !g_managed_fields_verified)
        return;
    p_field_get_value(self, g_station_field, &station);
    p_field_get_value(self, g_support_field, &support);
    if (station != placement_wait_item)
        return;
    if (support) {
        if (placement_support_cleared)
            return;
        void *clear = pf_method(p_object_get_class(self), "ClearSupport", "System.Void", 0, 0, 1);
        if (!clear || !write_code_guard(clear, 0x35a8270, 0x80, 0x070f24cfU)) {
            placement_log("AUTO IMPORT STOPPED: ClearSupport binary guard differs");
            placement_wait_clear();
            return;
        }
        placement_support_cleared = 1;
        if (!placement_clear_support(self, support, clear)) {
            placement_log("AUTO IMPORT STOPPED: support clearing failed");
            placement_wait_clear();
            return;
        }
        placement_cursor_ended = now;
        return;
    }
    if (now < placement_next_check)
        return;
    placement_next_check = now + 500;
    PFRoots check = {0};
    void *bus = 0;
    unsigned entity[2] = {0};
    p_field_get_value(station, g_entity_field, entity);
    void *context = write_context(station, entity, &check, &bus);
    int total = 0, active = 0;
    int ready = context && write_queue_state(bus, 0, &total, &active, 0) &&
                write_import_queue_ready(bus, 0);
    if (!ready && context && now >= placement_queue_diagnostic) {
        placement_queue_diagnostic = now + 2000;
        placement_log(
            "WAIT: exact station matched; event queue not idle (pending types follow when readable)");
        int queued = 0, processing = 0;
        write_queue_state(bus, 0, &queued, &processing, 1);
    }
    pf_release(&check);
    if (!ready)
        return;
    verify_support_orientation();
    placement_log(
        "AUTO IMPORT: placement cursor ended and coaster controls match the retained station; submitting once through the guarded importer");
    run_first_write(station);
    placement_import_pending = write_pending == 1;
    placement_wait_clear();
}
static void observed_placement_lateupdate(void *self, void *methodInfo) {
    if (!app_services_available()) {
        if (placement_original)
            placement_original(self, methodInfo);
        return;
    }
    if (footprint_go && p_thread_current())
        footprint_tick();
    __atomic_add_fetch(&placement_callbacks, 1, __ATOMIC_RELAXED);
    if (!placement_thread)
        placement_thread = GetCurrentThreadId();
    if (placement_wait_item && GetTickCount64() - placement_wait_started > 180000 &&
        p_thread_current()) {
        placement_log("AUTO IMPORT TIMEOUT or cancellation: releasing retained target");
        placement_wait_clear();
    }
    /* GameApp services verification only after coaster controls stop receiving callbacks. */
    if (write_pending && p_thread_current() && GetTickCount64() - write_queued_tick > 1000)
        service_pending_write();
    if (self && __atomic_exchange_n(&placement_request, 0, __ATOMIC_ACQ_REL))
        placement_report();
    if (self && p_string_new)
        coaster_ui_service();
    if (placement_original)
        placement_original(self, methodInfo);
}
static int install_placement_entry(void) {
    void *c = findclass(p_domain_get(), "TPS.Game", "GameApp");
    void *m = c ? pf_method(c, "LateUpdate", "System.Void", 0, 0, 1) : 0;
    if (!m) {
        logtext("UI DISABLED: GameApp.LateUpdate must be a unique non-generic instance void method with no arguments.\r\n");
        return 0;
    }
    void *original = 0;
    SIZE_T got = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), m, &original, sizeof(original), &got) ||
        got != sizeof(original))
        return 0;
    /* Forwarding this observer depends on the metadata ABI, not body bytes.
     * Capture/import retain their independent strict native guards. */
    if (!is_executable_game_pointer(original)) {
        logtext("UI DISABLED: GameApp.LateUpdate is not executable GameAssembly code.\r\n");
        return 0;
    }
    placement_original = (native_ui_lateupdate)original;
    DWORD protect = 0;
    if (!VirtualProtect(m, sizeof(void *), PAGE_READWRITE, &protect))
        return 0;
    void *expected = original;
    int installed =
        __atomic_compare_exchange_n((void **)m, &expected, (void *)observed_placement_lateupdate, 0,
                                    __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    DWORD ignored = 0;
    VirtualProtect(m, sizeof(void *), protect, &ignored);
    logtext(
        installed
            ? "UI GameApp.LateUpdate observer installed (metadata ABI and executable section verified; feature guards retained).\r\n"
            : "F10 DISABLED: concurrent method-slot change.\r\n");
    return installed;
}
