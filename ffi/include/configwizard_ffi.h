#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CwSession CwSession;

/// Returns an error message allocated with cw_string_free, or null if none.
char *cw_last_error(void);
void cw_string_free(char *value);

CwSession *cw_session_open(const char *schema_path);
void cw_session_close(CwSession *session);

/// Merge answer values from a JSON object into the session.
/// Returns 1 on success, 0 on failure.
int cw_session_sync_values(CwSession *session, const char *values_json);

/// JSON: { "title", "description", "groups": ["Project", ...] }
char *cw_session_metadata_json(CwSession *session);

/// JSON page payload for one wizard group.
char *cw_session_render_group_json(CwSession *session, const char *group_id, size_t index,
                                   size_t total);

#ifdef __cplusplus
}
#endif
