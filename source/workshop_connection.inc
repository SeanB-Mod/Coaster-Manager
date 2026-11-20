/* An owned Steam pipe isolates upload completions from Steamworks.NET's
 * game-owned manual dispatcher. Never drain the game's primary pipe. */
typedef struct {
    int user, callback;
    void *data;
    int size;
} WorkshopCallbackMessage;
typedef struct {
    WorkshopId call;
    int callback;
    unsigned size;
} WorkshopCallCompleted;
typedef char WorkshopCallbackMessageSize[sizeof(WorkshopCallbackMessage) == 24 ? 1 : -1];
typedef char WorkshopCallCompletedSize[sizeof(WorkshopCallCompleted) == 16 ? 1 : -1];
static void *workshop_client;
static int workshop_pipe, workshop_pipe_user;
static void *(*workshop_get_client)(void);
static int (*workshop_primary_pipe)(void);
static int (*workshop_create_pipe)(void *);
static int (*workshop_connect_user)(void *, int);
static WorkshopBool (*workshop_release_pipe)(void *, int);
static void (*workshop_release_user)(void *, int, int);
static void *(*workshop_pipe_ugc)(void *, int, int, const char *);
static void *(*workshop_pipe_utils)(void *, int, const char *);
static void *(*workshop_pipe_user_interface)(void *, int, int, const char *);
static void (*workshop_run_frame)(int);
static WorkshopBool (*workshop_next_callback)(int, WorkshopCallbackMessage *);
static void (*workshop_free_callback)(int);
static WorkshopBool (*workshop_get_call_result)(int, WorkshopId, void *, int, int, WorkshopBool *);
static WorkshopId (*workshop_details)(void *, WorkshopId, unsigned);
static unsigned char workshop_completion[65536];
static unsigned workshop_completion_size;
static int workshop_completion_callback, workshop_completion_ready;
static WorkshopBool workshop_completion_failed;
static void workshop_release_connection(void) {
    if (workshop_pipe && workshop_client) {
        if (workshop_pipe_user && workshop_release_user)
            workshop_release_user(workshop_client, workshop_pipe, workshop_pipe_user);
        if (workshop_release_pipe)
            workshop_release_pipe(workshop_client, workshop_pipe);
    }
    workshop_pipe = workshop_pipe_user = 0;
    workshop_client = 0;
    workshop_completion_ready = 0;
}
static int workshop_open_connection(HMODULE steam) {
#define WSC_BIND(name, symbol)                                                                     \
    *(void **)(&name) = (void *)GetProcAddress(steam, symbol);                                     \
    if (!name) {                                                                                   \
        logtext("WORKSHOP missing SDK export: ");                                                  \
        logtext(symbol);                                                                           \
        nl();                                                                                      \
        workshop_bind_error =                                                                      \
            "Steam Workshop SDK is incompatible with this game version. Check the mod log.";       \
        return 0;                                                                                  \
    }
    WSC_BIND(workshop_get_client, "SteamClient");
    WSC_BIND(workshop_primary_pipe, "SteamAPI_GetHSteamPipe");
    WSC_BIND(workshop_create_pipe, "SteamAPI_ISteamClient_CreateSteamPipe");
    WSC_BIND(workshop_connect_user, "SteamAPI_ISteamClient_ConnectToGlobalUser");
    WSC_BIND(workshop_release_pipe, "SteamAPI_ISteamClient_BReleaseSteamPipe");
    WSC_BIND(workshop_release_user, "SteamAPI_ISteamClient_ReleaseUser");
    WSC_BIND(workshop_pipe_ugc, "SteamAPI_ISteamClient_GetISteamUGC");
    WSC_BIND(workshop_pipe_utils, "SteamAPI_ISteamClient_GetISteamUtils");
    WSC_BIND(workshop_pipe_user_interface, "SteamAPI_ISteamClient_GetISteamUser");
    WSC_BIND(workshop_run_frame, "SteamAPI_ManualDispatch_RunFrame");
    WSC_BIND(workshop_next_callback, "SteamAPI_ManualDispatch_GetNextCallback");
    WSC_BIND(workshop_free_callback, "SteamAPI_ManualDispatch_FreeLastCallback");
    WSC_BIND(workshop_get_call_result, "SteamAPI_ManualDispatch_GetAPICallResult");
    WSC_BIND(workshop_details, "SteamAPI_ISteamUGC_RequestUGCDetails");
#undef WSC_BIND
    workshop_client = workshop_get_client();
    if (!workshop_client)
        return 0;
    workshop_pipe = workshop_create_pipe(workshop_client);
    if (!workshop_pipe ||
        workshop_pipe == workshop_primary_pipe()) { /* Never release a pipe owned by the game. */
        workshop_pipe = 0;
        workshop_client = 0;
        return 0;
    }
    workshop_pipe_user = workshop_connect_user(workshop_client, workshop_pipe);
    if (!workshop_pipe_user)
        goto fail;
    workshop_ugc = workshop_pipe_ugc(workshop_client, workshop_pipe_user, workshop_pipe,
                                     "STEAMUGC_INTERFACE_VERSION020");
    workshop_utils = workshop_pipe_utils(workshop_client, workshop_pipe, "SteamUtils010");
    workshop_user = workshop_pipe_user_interface(workshop_client, workshop_pipe_user, workshop_pipe,
                                                 "SteamUser023");
    if (!workshop_ugc || !workshop_utils || !workshop_user) {
        workshop_bind_error =
            "Steam Workshop interfaces are incompatible with this game version. Check the mod log.";
        logtext("WORKSHOP: required UGC020/Utils010/User023 interface unavailable.\r\n");
        goto fail;
    }
    if (workshop_app(workshop_utils) != 2185060 || !workshop_logged_on(workshop_user) ||
        !workshop_owner(workshop_user))
        goto fail;
    logtext("WORKSHOP: isolated Steam pipe ready; game callback queue untouched.\r\n");
    return 1;
fail:
    workshop_release_connection();
    return 0;
}
static WorkshopBool workshop_owned_completed(void *self, WorkshopId call, WorkshopBool *failed) {
    (void)self;
    *failed = 0;
    if (!workshop_pipe || workshop_pipe == workshop_primary_pipe())
        return 0;
    if (!workshop_completion_ready) {
        workshop_run_frame(workshop_pipe);
        for (unsigned i = 0; i < 64; ++i) {
            WorkshopCallbackMessage message = {0};
            if (!workshop_next_callback(workshop_pipe, &message))
                break;
            if (message.callback == 703 && message.size == (int)sizeof(WorkshopCallCompleted) &&
                message.data) {
                WorkshopCallCompleted complete;
                memcpy(&complete, message.data, sizeof(complete));
                if (complete.call == call) {
                    int expected = workshop_phase == 1   ? 3403
                                   : workshop_phase == 2 ? 3404
                                   : workshop_phase == 4 ? 3402
                                   : workshop_phase == 5 ? 3412
                                   : workshop_phase == 6 ? 3414
                                                         : 0;
                    int sizeOK =
                        complete.callback == expected &&
                        (expected == 3403                         ? complete.size == 24
                         : expected == 3404                       ? complete.size == 16
                         : (expected == 3412 || expected == 3414) ? complete.size == 24
                         : expected == 3402
                             ? complete.size >= 24 && complete.size <= sizeof(workshop_completion)
                             : 0);
                    workshop_completion_size = complete.size;
                    workshop_completion_callback = complete.callback;
                    workshop_completion_failed = 0;
                    if (!sizeOK || !workshop_get_call_result(
                                       workshop_pipe, call, workshop_completion, (int)complete.size,
                                       complete.callback, &workshop_completion_failed))
                        workshop_completion_failed = 1;
                    workshop_completion_ready = 1;
                    logtext("WORKSHOP COMPLETION: phase=");
                    lognum((UINT_PTR)workshop_phase);
                    logtext(" callback=");
                    lognum((UINT_PTR)complete.callback);
                    logtext(" bytes=");
                    lognum(complete.size);
                    logtext(" failed=");
                    lognum(workshop_completion_failed);
                    nl();
                }
            }
            workshop_free_callback(workshop_pipe);
            if (workshop_completion_ready)
                break;
        }
    }
    *failed = workshop_completion_failed;
    return (WorkshopBool)workshop_completion_ready;
}
static WorkshopBool workshop_owned_result(void *self, WorkshopId call, void *out, int size,
                                          int callback, WorkshopBool *failed) {
    (void)self;
    (void)call;
    *failed = workshop_completion_failed;
    if (!workshop_completion_ready || workshop_completion_failed ||
        callback != workshop_completion_callback || size < 0 ||
        (unsigned)size != workshop_completion_size)
        return 0;
    memcpy(out, workshop_completion, (unsigned)size);
    workshop_completion_ready = 0;
    return 1;
}
