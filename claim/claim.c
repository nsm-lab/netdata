// SPDX-License-Identifier: GPL-3.0-or-later

#include "claim.h"
#include "../registry/registry_internals.h"
#include "../aclk/aclk_common.h"

char *claiming_pending_arguments = NULL;

static char *claiming_errors[] = {
        "Agent claimed successfully",                   // 0
        "Unknown argument",                             // 1
        "Problems with claiming working directory",     // 2
        "Missing dependencies",                         // 3
        "Failure to connect to endpoint",               // 4
        "Unknown HTTP error message",                   // 5
        "invalid node id",                              // 6
        "invalid node name",                            // 7
        "invalid room id",                              // 8
        "invalid public key",                           // 9
        "token expired/token not found/invalid token",  // 10
        "already claimed",                              // 11
        "processing claiming",                          // 12
        "Internal Server Error",                        // 13
        "Gateway Timeout",                              // 14
        "Service Unavailable"                           // 15
};


static char *claimed_id = NULL;

char *is_agent_claimed(void)
{
    return claimed_id;
}

#define CLAIMING_COMMAND_LENGTH 16384
#define CLAIMING_PROXY_LENGTH CLAIMING_COMMAND_LENGTH/4

extern struct registry registry;

/* rrd_init() must have been called before this function */
void claim_agent(char *claiming_arguments)
{
#ifndef ENABLE_CLOUD
    info("The claiming feature has been disabled");
    return;
#endif

    int exit_code;
    pid_t command_pid;
    char command_buffer[CLAIMING_COMMAND_LENGTH + 1];
    FILE *fp;

    char *cloud_base_hostname = NULL; // Initializers are over-written but prevent gcc complaining about clobbering.
    char *cloud_base_port = NULL;
    char *cloud_base_url = config_get(CONFIG_SECTION_CLOUD, "cloud base url", "https://netdata.cloud");
    if( aclk_decode_base_url(cloud_base_url, &cloud_base_hostname, &cloud_base_port))
    {
        error("Configuration error - cannot decode \"cloud base url\"");
        return;
    }

    const char *proxy_str;
    ACLK_PROXY_TYPE proxy_type;
    char proxy_flag[CLAIMING_PROXY_LENGTH] = "-noproxy";

    proxy_str = aclk_get_proxy(&proxy_type);

    if (proxy_type == PROXY_TYPE_SOCKS5 || proxy_type == PROXY_TYPE_HTTP)
        snprintf(proxy_flag, CLAIMING_PROXY_LENGTH, "-proxy=\"%s\"", proxy_str);

    snprintfz(command_buffer,
              CLAIMING_COMMAND_LENGTH,
              "exec netdata-claim.sh %s -hostname=%s -id=%s -url=%s %s",

              proxy_flag,
              netdata_configured_hostname,
              localhost->machine_guid,
              cloud_base_url,
              claiming_arguments);

    info("Executing agent claiming command 'netdata-claim.sh'");
    fp = mypopen(command_buffer, &command_pid);
    if(!fp) {
        error("Cannot popen(\"%s\").", command_buffer);
        return;
    }
    info("Waiting for claiming command to finish.");
    while (fgets(command_buffer, CLAIMING_COMMAND_LENGTH, fp) != NULL) {;}
    exit_code = mypclose(fp, command_pid);
    info("Agent claiming command returned with code %d", exit_code);
    if (0 == exit_code) {
        load_claiming_state();
        return;
    }
    if (exit_code < 0) {
        error("Agent claiming command failed to complete its run.");
        return;
    }
    errno = 0;
    unsigned maximum_known_exit_code = sizeof(claiming_errors) / sizeof(claiming_errors[0]) - 1;

    if ((unsigned)exit_code > maximum_known_exit_code) {
        error("Agent failed to be claimed with an unknown error.");
        return;
    }
    error("Agent failed to be claimed with the following error message:");
    error("\"%s\"", claiming_errors[exit_code]);
}

void load_claiming_state(void)
{
    if (claimed_id != NULL) {
        freez(claimed_id);
        claimed_id = NULL;
    }

    char filename[FILENAME_MAX + 1];
    struct stat statbuf;

    snprintfz(filename, FILENAME_MAX, "%s/claim.d/claimed_id", netdata_configured_user_config_dir);

    // check if the file exists
    if (lstat(filename, &statbuf) != 0) {
        info("lstat on File '%s' failed reason=\"%s\". Setting state to AGENT_UNCLAIMED.", filename, strerror(errno));
        return;
    }
    if (unlikely(statbuf.st_size == 0)) {
        info("File '%s' has no contents. Setting state to AGENT_UNCLAIMED.", filename);
        return;
    }

    FILE *f = fopen(filename, "rt");
    if (unlikely(f == NULL)) {
        error("File '%s' cannot be opened. Setting state to AGENT_UNCLAIMED.", filename);
        return;
    }

    claimed_id = callocz(1, statbuf.st_size + 1);
    size_t bytes_read = fread(claimed_id, 1, statbuf.st_size, f);
    claimed_id[bytes_read] = 0;
    info("File '%s' was found. Setting state to AGENT_CLAIMED.", filename);
    fclose(f);

    snprintfz(filename, FILENAME_MAX, "%s/claim.d/private.pem", netdata_configured_user_config_dir);
}
