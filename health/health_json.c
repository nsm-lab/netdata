// SPDX-License-Identifier: GPL-3.0-or-later

#include "health.h"

static inline void health_string2json(BUFFER *wb, const char *prefix, const char *label, const char *value, const char *suffix) {
    if(value && *value) {
        buffer_sprintf(wb, "%s\"%s\":\"", prefix, label);
        buffer_strcat_htmlescape(wb, value);
        buffer_strcat(wb, "\"");
        buffer_strcat(wb, suffix);
    }
    else
        buffer_sprintf(wb, "%s\"%s\":null%s", prefix, label, suffix);
}

inline void health_alarm_entry2json_nolock(BUFFER *wb, ALARM_ENTRY *ae, RRDHOST *host) {
    buffer_sprintf(wb,
            "\n\t{\n"
                    "\t\t\"hostname\": \"%s\",\n"
                    "\t\t\"unique_id\": %u,\n"
                    "\t\t\"alarm_id\": %u,\n"
                    "\t\t\"alarm_event_id\": %u,\n"
                    "\t\t\"name\": \"%s\",\n"
                    "\t\t\"chart\": \"%s\",\n"
                    "\t\t\"family\": \"%s\",\n"
                    "\t\t\"processed\": %s,\n"
                    "\t\t\"updated\": %s,\n"
                    "\t\t\"exec_run\": %lu,\n"
                    "\t\t\"exec_failed\": %s,\n"
                    "\t\t\"exec\": \"%s\",\n"
                    "\t\t\"recipient\": \"%s\",\n"
                    "\t\t\"exec_code\": %d,\n"
                    "\t\t\"source\": \"%s\",\n"
                    "\t\t\"units\": \"%s\",\n"
                    "\t\t\"when\": %lu,\n"
                    "\t\t\"duration\": %lu,\n"
                    "\t\t\"non_clear_duration\": %lu,\n"
                    "\t\t\"status\": \"%s\",\n"
                    "\t\t\"old_status\": \"%s\",\n"
                    "\t\t\"delay\": %d,\n"
                    "\t\t\"delay_up_to_timestamp\": %lu,\n"
                    "\t\t\"updated_by_id\": %u,\n"
                    "\t\t\"updates_id\": %u,\n"
                    "\t\t\"value_string\": \"%s\",\n"
                    "\t\t\"old_value_string\": \"%s\",\n"
                    "\t\t\"last_repeat\": \"%lu\",\n"
                    "\t\t\"silenced\": \"%s\",\n"
                   , host->hostname
                   , ae->unique_id
                   , ae->alarm_id
                   , ae->alarm_event_id
                   , ae->name
                   , ae->chart
                   , ae->family
                   , (ae->flags & HEALTH_ENTRY_FLAG_PROCESSED)?"true":"false"
                   , (ae->flags & HEALTH_ENTRY_FLAG_UPDATED)?"true":"false"
                   , (unsigned long)ae->exec_run_timestamp
                   , (ae->flags & HEALTH_ENTRY_FLAG_EXEC_FAILED)?"true":"false"
                   , ae->exec?ae->exec:host->health_default_exec
                   , ae->recipient?ae->recipient:host->health_default_recipient
                   , ae->exec_code
                   , ae->source
                   , ae->units?ae->units:""
                   , (unsigned long)ae->when
                   , (unsigned long)ae->duration
                   , (unsigned long)ae->non_clear_duration
                   , rrdcalc_status2string(ae->new_status)
                   , rrdcalc_status2string(ae->old_status)
                   , ae->delay
                   , (unsigned long)ae->delay_up_to_timestamp
                   , ae->updated_by_id
                   , ae->updates_id
                   , ae->new_value_string
                   , ae->old_value_string
                   , (unsigned long)ae->last_repeat
                   , (ae->flags & HEALTH_ENTRY_FLAG_SILENCED)?"true":"false"
    );

    health_string2json(wb, "\t\t", "info", ae->info?ae->info:"", ",\n");

    if(unlikely(ae->flags & HEALTH_ENTRY_FLAG_NO_CLEAR_NOTIFICATION)) {
        buffer_strcat(wb, "\t\t\"no_clear_notification\": true,\n");
    }

    buffer_strcat(wb, "\t\t\"value\":");
    buffer_rrd_value(wb, ae->new_value);
    buffer_strcat(wb, ",\n");

    buffer_strcat(wb, "\t\t\"old_value\":");
    buffer_rrd_value(wb, ae->old_value);
    buffer_strcat(wb, "\n");

    buffer_strcat(wb, "\t}");
}

void health_alarm_log2json(RRDHOST *host, BUFFER *wb, uint32_t after) {
    netdata_rwlock_rdlock(&host->health_log.alarm_log_rwlock);

    buffer_strcat(wb, "[");

    unsigned int max = host->health_log.max;
    unsigned int count = 0;
    ALARM_ENTRY *ae;
    for(ae = host->health_log.alarms; ae && count < max ; count++, ae = ae->next) {
        if(ae->unique_id > after) {
            if(likely(count)) buffer_strcat(wb, ",");
            health_alarm_entry2json_nolock(wb, ae, host);
        }
    }

    buffer_strcat(wb, "\n]\n");

    netdata_rwlock_unlock(&host->health_log.alarm_log_rwlock);
}

static inline void health_rrdcalc_values2json_nolock(RRDHOST *host, BUFFER *wb, RRDCALC *rc) {
    (void)host;
    buffer_sprintf(wb,
                   "\t\t\"%s.%s\": {\n"
                   "\t\t\t\"id\": %lu,\n"
                   , rc->chart, rc->name
                   , (unsigned long)rc->id);

    buffer_strcat(wb, "\t\t\t\"value\":");
    buffer_rrd_value(wb, rc->value);
    buffer_strcat(wb, ",\n");

    buffer_sprintf(wb,
                   "\t\t\t\"status\": \"%s\"\n"
                   , rrdcalc_status2string(rc->status));

    buffer_strcat(wb, "\t\t}");
}

static inline void health_rrdcalc2json_nolock(RRDHOST *host, BUFFER *wb, RRDCALC *rc) {
    char value_string[100 + 1];
    format_value_and_unit(value_string, 100, rc->value, rc->units, -1);

    buffer_sprintf(wb,
            "\t\t\"%s.%s\": {\n"
                    "\t\t\t\"id\": %lu,\n"
                    "\t\t\t\"name\": \"%s\",\n"
                    "\t\t\t\"chart\": \"%s\",\n"
                    "\t\t\t\"family\": \"%s\",\n"
                    "\t\t\t\"active\": %s,\n"
                    "\t\t\t\"disabled\": %s,\n"
                    "\t\t\t\"silenced\": %s,\n"
                    "\t\t\t\"exec\": \"%s\",\n"
                    "\t\t\t\"recipient\": \"%s\",\n"
                    "\t\t\t\"source\": \"%s\",\n"
                    "\t\t\t\"units\": \"%s\",\n"
                    "\t\t\t\"info\": \"%s\",\n"
                    "\t\t\t\"status\": \"%s\",\n"
                    "\t\t\t\"last_status_change\": %lu,\n"
                    "\t\t\t\"last_updated\": %lu,\n"
                    "\t\t\t\"next_update\": %lu,\n"
                    "\t\t\t\"update_every\": %d,\n"
                    "\t\t\t\"delay_up_duration\": %d,\n"
                    "\t\t\t\"delay_down_duration\": %d,\n"
                    "\t\t\t\"delay_max_duration\": %d,\n"
                    "\t\t\t\"delay_multiplier\": %f,\n"
                    "\t\t\t\"delay\": %d,\n"
                    "\t\t\t\"delay_up_to_timestamp\": %lu,\n"
                    "\t\t\t\"warn_repeat_every\": \"%u\",\n"
                    "\t\t\t\"crit_repeat_every\": \"%u\",\n"
                    "\t\t\t\"value_string\": \"%s\",\n"
                    "\t\t\t\"last_repeat\": \"%lu\",\n"
                   , rc->chart, rc->name
                   , (unsigned long)rc->id
                   , rc->name
                   , rc->chart
                   , (rc->rrdset && rc->rrdset->family)?rc->rrdset->family:""
                   , (rc->rrdset)?"true":"false"
                   , (rc->rrdcalc_flags & RRDCALC_FLAG_DISABLED)?"true":"false"
                   , (rc->rrdcalc_flags & RRDCALC_FLAG_SILENCED)?"true":"false"
                   , rc->exec?rc->exec:host->health_default_exec
                   , rc->recipient?rc->recipient:host->health_default_recipient
                   , rc->source
                   , rc->units?rc->units:""
                   , rc->info?rc->info:""
                   , rrdcalc_status2string(rc->status)
                   , (unsigned long)rc->last_status_change
                   , (unsigned long)rc->last_updated
                   , (unsigned long)rc->next_update
                   , rc->update_every
                   , rc->delay_up_duration
                   , rc->delay_down_duration
                   , rc->delay_max_duration
                   , rc->delay_multiplier
                   , rc->delay_last
                   , (unsigned long)rc->delay_up_to_timestamp
                   , rc->warn_repeat_every
                   , rc->crit_repeat_every
                   , value_string
                   , (unsigned long)rc->last_repeat
    );

    if(unlikely(rc->options & RRDCALC_FLAG_NO_CLEAR_NOTIFICATION)) {
        buffer_strcat(wb, "\t\t\t\"no_clear_notification\": true,\n");
    }

    if(RRDCALC_HAS_DB_LOOKUP(rc)) {
        if(rc->dimensions && *rc->dimensions)
            health_string2json(wb, "\t\t\t", "lookup_dimensions", rc->dimensions, ",\n");

        buffer_sprintf(wb,
                "\t\t\t\"db_after\": %lu,\n"
                        "\t\t\t\"db_before\": %lu,\n"
                        "\t\t\t\"lookup_method\": \"%s\",\n"
                        "\t\t\t\"lookup_after\": %d,\n"
                        "\t\t\t\"lookup_before\": %d,\n"
                        "\t\t\t\"lookup_options\": \"",
                (unsigned long) rc->db_after,
                (unsigned long) rc->db_before,
                group_method2string(rc->group),
                rc->after,
                rc->before
        );
        buffer_data_options2string(wb, rc->options);
        buffer_strcat(wb, "\",\n");
    }

    if(rc->calculation) {
        health_string2json(wb, "\t\t\t", "calc", rc->calculation->source, ",\n");
        health_string2json(wb, "\t\t\t", "calc_parsed", rc->calculation->parsed_as, ",\n");
    }

    if(rc->warning) {
        health_string2json(wb, "\t\t\t", "warn", rc->warning->source, ",\n");
        health_string2json(wb, "\t\t\t", "warn_parsed", rc->warning->parsed_as, ",\n");
    }

    if(rc->critical) {
        health_string2json(wb, "\t\t\t", "crit", rc->critical->source, ",\n");
        health_string2json(wb, "\t\t\t", "crit_parsed", rc->critical->parsed_as, ",\n");
    }

    buffer_strcat(wb, "\t\t\t\"green\":");
    buffer_rrd_value(wb, rc->green);
    buffer_strcat(wb, ",\n");

    buffer_strcat(wb, "\t\t\t\"red\":");
    buffer_rrd_value(wb, rc->red);
    buffer_strcat(wb, ",\n");

    buffer_strcat(wb, "\t\t\t\"value\":");
    buffer_rrd_value(wb, rc->value);
    buffer_strcat(wb, "\n");

    buffer_strcat(wb, "\t\t}");
}

//void health_rrdcalctemplate2json_nolock(BUFFER *wb, RRDCALCTEMPLATE *rt) {
//
//}

void health_aggregate_alarms(RRDHOST *host, BUFFER *wb, BUFFER* contexts, RRDCALC_STATUS status) {
    RRDCALC *rc;
    int numberOfAlarms = 0;
    char *tok = NULL;
    char *p = NULL;

    rrdhost_rdlock(host);

    if (contexts) {
        p = (char*)buffer_tostring(contexts);
        while(p && *p && (tok = mystrsep(&p, ", |"))) {
            if(!*tok) continue;

            for(rc = host->alarms; rc ; rc = rc->next) {
                if(unlikely(!rc->rrdset || !rc->rrdset->last_collected_time.tv_sec))
                    continue;
                if(unlikely(rc->rrdset && rc->rrdset->hash_context == simple_hash(tok)
                            && !strcmp(rc->rrdset->context, tok)
                            && ((status==RRDCALC_STATUS_RAISED)?(rc->status >= RRDCALC_STATUS_WARNING):rc->status == status)))
                    numberOfAlarms++;
            }
        }
    }
    else {
        for(rc = host->alarms; rc ; rc = rc->next) {
            if(unlikely(!rc->rrdset || !rc->rrdset->last_collected_time.tv_sec))
                continue;

            if(unlikely((status==RRDCALC_STATUS_RAISED)?(rc->status >= RRDCALC_STATUS_WARNING):rc->status == status))
                numberOfAlarms++;
        }
    }

    buffer_sprintf(wb, "%d", numberOfAlarms);
    rrdhost_unlock(host);
}

void health_alarms2json_fill_alarms(RRDHOST *host, BUFFER *wb, int all, void (*fp)(RRDHOST *, BUFFER *, RRDCALC *)) {
    RRDCALC *rc;
    int i;
    for(i = 0, rc = host->alarms; rc ; rc = rc->next) {
        if(unlikely(!rc->rrdset || !rc->rrdset->last_collected_time.tv_sec))
            continue;

        if(likely(!all && !(rc->status == RRDCALC_STATUS_WARNING || rc->status == RRDCALC_STATUS_CRITICAL)))
            continue;

        if(likely(i)) buffer_strcat(wb, ",\n");
        fp(host, wb, rc);
        i++;
    }
}

void health_alarms2json(RRDHOST *host, BUFFER *wb, int all) {
    rrdhost_rdlock(host);
    buffer_sprintf(wb, "{\n\t\"hostname\": \"%s\","
                    "\n\t\"latest_alarm_log_unique_id\": %u,"
                    "\n\t\"status\": %s,"
                    "\n\t\"now\": %lu,"
                    "\n\t\"alarms\": {\n",
            host->hostname,
            (host->health_log.next_log_id > 0)?(host->health_log.next_log_id - 1):0,
            host->health_enabled?"true":"false",
            (unsigned long)now_realtime_sec());

    health_alarms2json_fill_alarms(host, wb, all,  health_rrdcalc2json_nolock);

//    buffer_strcat(wb, "\n\t},\n\t\"templates\": {");
//    RRDCALCTEMPLATE *rt;
//    for(rt = host->templates; rt ; rt = rt->next)
//        health_rrdcalctemplate2json_nolock(wb, rt);

    buffer_strcat(wb, "\n\t}\n}\n");
    rrdhost_unlock(host);
}

void health_alarms_values2json(RRDHOST *host, BUFFER *wb, int all) {
    rrdhost_rdlock(host);
    buffer_sprintf(wb, "{\n\t\"hostname\": \"%s\","
                       "\n\t\"alarms\": {\n",
                   host->hostname);

    health_alarms2json_fill_alarms(host, wb, all,  health_rrdcalc_values2json_nolock);

    buffer_strcat(wb, "\n\t}\n}\n");
    rrdhost_unlock(host);
}
