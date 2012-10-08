/******************************************************************************/
/* config.c  -- Functions dealing with the config file
 *
 * Copyright 2012 The AOL Moloch Authors.  All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "glib.h"
#include "moloch.h"

extern char          *configFile;
extern char          *nodeName;
extern gchar         *pcapFile;
extern gboolean       debug;
extern MolochConfig_t config;

/******************************************************************************/
gchar *moloch_config_str(GKeyFile *keyfile, char *key, char *d)
{
    if (g_key_file_has_key(keyfile, nodeName, key, NULL)) {
        return g_key_file_get_string(keyfile, nodeName, key, NULL);
    }

    if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        return g_key_file_get_string(keyfile, config.nodeClass, key, NULL);
    }

    if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        return g_key_file_get_string(keyfile, "default", key, NULL);
    }

    if (!d)
        return NULL;

    return g_strdup(d);
}

/******************************************************************************/
gchar **moloch_config_str_list(GKeyFile *keyfile, char *key, char *d)
{
    if (g_key_file_has_key(keyfile, nodeName, key, NULL)) {
        return g_key_file_get_string_list(keyfile, nodeName, key, NULL, NULL);
    }

    if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        return g_key_file_get_string_list(keyfile, config.nodeClass, key, NULL, NULL);
    }

    if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        return g_key_file_get_string_list(keyfile, "default", key, NULL, NULL);
    }

    if (!d)
        return NULL;

    return g_strsplit(d, ";", 0);
}

/******************************************************************************/
uint32_t moloch_config_int(GKeyFile *keyfile, char *key, uint32_t d, uint32_t min, uint32_t max)
{
    uint32_t value = d;

    if (g_key_file_has_key(keyfile, nodeName, key, NULL)) {
        value = g_key_file_get_integer(keyfile, nodeName, key, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        value = g_key_file_get_integer(keyfile, config.nodeClass, key, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        value = g_key_file_get_integer(keyfile, "default", key, NULL);
    }

    if (value < min)
        value = min;
    if (value > max)
        value = max;

    return value;
}

/******************************************************************************/
char moloch_config_boolean(GKeyFile *keyfile, char *key, char d)
{
    gboolean value = d;

    if (g_key_file_has_key(keyfile, nodeName, key, NULL)) {
        value = g_key_file_get_boolean(keyfile, nodeName, key, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        value = g_key_file_get_boolean(keyfile, config.nodeClass, key, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        value = g_key_file_get_boolean(keyfile, "default", key, NULL);
    }

    return value;
}

/******************************************************************************/
void moloch_config_load() 
{

    gboolean  status;
    GKeyFile *keyfile = g_key_file_new();
    GError   *error = 0;

    status = g_key_file_load_from_file(keyfile, configFile, G_KEY_FILE_NONE, &error);
    if (!status || error) {
        printf("Couldn't load config file (%s) %s\n", configFile, (error?error->message:""));
        exit(1);
    }

    gchar **tags            = moloch_config_str_list(keyfile, "dontSaveTags", NULL);
    if (tags) {
        int i;
        for (i = 0; tags[i]; i++) {
            MolochString_t *tstring;
            gchar *tag = tags[i];
            while (isspace(*tag))
                tag++;
            g_strchomp(tag);

            if (!(*tag))
                continue;

            HASH_FIND(s_, config.dontSaveTags, tag, tstring);
            if (!tstring) {
                tstring = malloc(sizeof(*tstring));
                tstring->str = g_strdup(tag);
                HASH_ADD(s_, config.dontSaveTags, tstring->str, tstring);
            }
        }
        g_strfreev(tags);
    }

    config.nodeClass        = moloch_config_str(keyfile, "nodeClass", NULL);

    config.elasticsearch    = moloch_config_str(keyfile, "elasticsearch", "localhost:9200");
    config.interface        = moloch_config_str(keyfile, "interface", NULL);
    config.pcapDir          = moloch_config_str(keyfile, "pcapDir", NULL);
    config.bpf              = moloch_config_str(keyfile, "bpf", NULL);
    config.yara             = moloch_config_str(keyfile, "yara", NULL);
    config.geoipFile        = moloch_config_str(keyfile, "geoipFile", NULL);
    config.geoipASNFile     = moloch_config_str(keyfile, "geoipASNFile", NULL);
    config.dropUser         = moloch_config_str(keyfile, "dropUser", NULL);
    config.dropGroup        = moloch_config_str(keyfile, "dropGroup", NULL);

    config.maxFileSizeG     = moloch_config_int(keyfile, "maxFileSizeG", 4, 1, 63);
    config.icmpTimeout      = moloch_config_int(keyfile, "icmpTimeout", 10, 1, 0xffff);
    config.udpTimeout       = moloch_config_int(keyfile, "udpTimeout", 60, 1, 0xffff);
    config.tcpTimeout       = moloch_config_int(keyfile, "tcpTimeout", 60*8, 10, 0xffff);
    config.tcpSaveTimeout   = moloch_config_int(keyfile, "tcpSaveTimeout", 60*8, 10, 60*120);
    config.maxStreams       = moloch_config_int(keyfile, "maxStreams", 1500000, 1, 16777215);
    config.maxPackets       = moloch_config_int(keyfile, "maxPackets", 10000, 1, 1000000);
    config.minFreeSpaceG    = moloch_config_int(keyfile, "freeSpaceG", 100, 1, 100000);
    config.dbBulkSize       = moloch_config_int(keyfile, "dbBulkSize", 200000, 1, 1000000);
    config.maxESConns       = moloch_config_int(keyfile, "maxESConns", 100, 10, 1000);
    config.maxESRequests    = moloch_config_int(keyfile, "maxESRequests", 500, 10, 5000);
    config.logEveryXPackets = moloch_config_int(keyfile, "logEveryXPackets", 50000, 1000, 1000000);
    config.packetsPerPoll   = moloch_config_int(keyfile, "packetsPerPoll", 50000, 1000, 1000000);
    config.pcapBufferSize   = moloch_config_int(keyfile, "pcapBufferSize", 300000000, 100000, 0xffffffff);
    config.pcapWriteSize    = moloch_config_int(keyfile, "pcapWriteSize", 0x40000, 0x40000, 0x200000);


    config.logUnknownProtocols   = moloch_config_boolean(keyfile, "logUnknownProtocols", debug);
    config.logESRequests         = moloch_config_boolean(keyfile, "logESRequests", debug);
    config.logFileCreation       = moloch_config_boolean(keyfile, "logFileCreation", debug);

    g_key_file_free(keyfile);

}
/******************************************************************************/
void moloch_config_init()
{
    HASH_INIT(s_, config.dontSaveTags, moloch_string_hash, moloch_string_cmp);

    moloch_config_load();

    if (debug) {
        LOG("nodeClass: %s", config.nodeClass);
        LOG("elasticsearch: %s", config.elasticsearch);
        LOG("interface: %s", config.interface);
        LOG("pcapDir: %s", config.pcapDir);
        LOG("bpf: %s", config.bpf);
        LOG("yara: %s", config.yara);
        LOG("geoipFile: %s", config.geoipFile);
        LOG("geoipASNFile: %s", config.geoipASNFile);
        LOG("dropUser: %s", config.dropUser);
        LOG("dropGroup: %s", config.dropGroup);

        LOG("maxFileSizeG: %u", config.maxFileSizeG);
        LOG("icmpTimeout: %u", config.icmpTimeout);
        LOG("udpTimeout: %u", config.udpTimeout);
        LOG("tcpTimeout: %u", config.tcpTimeout);
        LOG("tcpSaveTimeout: %u", config.tcpSaveTimeout);
        LOG("maxStreams: %u", config.maxStreams);
        LOG("maxPackets: %u", config.maxPackets);
        LOG("minFreeSpaceG: %u", config.minFreeSpaceG);
        LOG("dbBulkSize: %u", config.dbBulkSize);
        LOG("maxESConns: %u", config.maxESConns);
        LOG("maxESRequests: %u", config.maxESRequests);
        LOG("logEveryXPackets: %u", config.logEveryXPackets);
        LOG("packetsPerPoll: %u", config.packetsPerPoll);
        LOG("pcapBufferSize: %u", config.pcapBufferSize);
        LOG("pcapWriteSize: %u", config.pcapWriteSize);

        LOG("logUnknownProtocols: %s", (config.logUnknownProtocols?"true":"false"));
        LOG("logESRequests: %s", (config.logESRequests?"true":"false"));
        LOG("logFileCreation: %s", (config.logFileCreation?"true":"false"));

        MolochString_t *tstring;
        HASH_FORALL(s_, config.dontSaveTags, tstring, 
          LOG("dontSaveTags: %s", tstring->str);
        );
    }

    if (!config.interface && !pcapFile) {
        printf("Need to set interface or pcapfile\n");
        exit (1);
    }
}
/******************************************************************************/
void moloch_config_exit()
{
    if (config.nodeClass)
        g_free(config.nodeClass);
    if (config.interface)
        g_free(config.interface);
    if (config.elasticsearch)
        g_free(config.elasticsearch);
    if (config.bpf)
        g_free(config.bpf);
    if (config.yara)
        g_free(config.yara);
    if (config.pcapDir)
        g_free(config.pcapDir);
}