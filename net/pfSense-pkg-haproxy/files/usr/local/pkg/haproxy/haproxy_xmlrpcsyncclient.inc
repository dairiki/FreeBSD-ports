<?php
/*
 * haproxy_xmlrpcsyncclient.inc
 *
 * part of pfSense (https://www.pfsense.org)
 * Copyright (c) 2009-2024 Rubicon Communications, LLC (Netgate)
 * Copyright (c) 2013 PiBa-NL
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* include all configuration functions */
require_once("functions.inc");
require_once("pkg-utils.inc");
require_once("notices.inc");

if(!function_exists('pf_version')) {
	function pf_version() {
		return substr(trim(file_get_contents("/etc/version")), 0, 3);
	}
}

function xmlrpc_sync_execute($syncinfo) {
	// name that is logged if something fails during syncing
	$sync_logname  = $syncinfo['sync_logname'];
	// configuration data to sync
	$xml           = $syncinfo['data'];
	// include file in which the "function sync_done_execute(){xxx}" must be pressent
	$sync_include  = $syncinfo['sync_include'];
	// executes to apply the changed configuration on the target system
	$sync_function = $syncinfo['sync_done_execute'];
	
	if (pf_version() < "2.4") {
		xmlrpc_sync_execute_23($sync_logname, $xml, $sync_include, $sync_function);
	} else {
		xmlrpc_sync_execute_24($sync_logname, $xml, $sync_include, $sync_function);
	}
}

function xmlrpc_sync_execute_24($sync_logname, $xml, $sync_include,$sync_function) {
	global $config;
	if (!is_array($config['hasync']) || empty($config['hasync']['synchronizetoip'])) {
		syslog(LOG_NOTICE, "haproxy: cannot sync configuration (no ip/host configured to sync to on this machine), disable sync to avoid this message.");
		return;
	}
	
	require_once("xmlrpc_client.inc");
	
	$rpc_client = new pfsense_xmlrpc_client();
	$resp = $rpc_client->xmlrpc_method('merge_installedpackages_section', $xml);
	
	$execcmd  = "require_once('{$sync_include}');\n";
	$execcmd .= "{$sync_function}();\n";		
	$resp = $rpc_client->xmlrpc_exec_php($execcmd);
}

function xmlrpc_sync_execute_23($sync_logname, $xml, $sync_include,$sync_function) {
	global $config, $g;
	//if(!$password)
		$password = $config['hasync']['password'];
	if(!$password) {
		return;
	}

	//if(!$sync_to_ip)
		$sync_to_ip = $config['hasync']['synchronizetoip'];
	if(!$sync_to_ip) {
		return;
	}
	
	// Do not allow syncing to self.
	$donotsync = false;
	$localips = get_configured_ip_addresses();
	if (in_array($sync_to_ip, $localips , true)) {
		$donotsync = true;
	}
	
	if($donotsync) {
		log_error("Disallowing sync loop for {$sync_logname} sync.");
		return;
	}

	require_once("xmlrpc.inc");
	require_once("xmlrpc_client.inc");
	
	//$xmlrpc_sync_neighbor = $sync_to_ip;
    if($config['system']['webgui']['protocol'] != "") {
		$synchronizetoip = $config['system']['webgui']['protocol'];
		$synchronizetoip .= "://";
    }
    $port = $config['system']['webgui']['port'];
    /* if port is empty lets rely on the protocol selection */
    if($port == "") {
		if($config['system']['webgui']['protocol'] == "http") {
			$port = "80";
		} else {
			$port = "443";
		}
    }
	$synchronizetoip .= $sync_to_ip;

	/* xml will hold the sections to sync */
	//$xml = prepare_xmlrpc_sync_config();


	/* assemble xmlrpc payload */
	$params = array(
		XML_RPC_encode($password),
		XML_RPC_encode($xml)
	);

	/* set a few variables needed for sync code borrowed from filter.inc */
	$url = $synchronizetoip;
	log_error("Beginning {$sync_logname} XMLRPC sync to {$url}:{$port}.");
	$method = 'pfsense.merge_installedpackages_section_xmlrpc';
	$msg = new XML_RPC_Message($method, $params);
	$cli = new XML_RPC_Client('/xmlrpc.php', $url, $port);
	$cli->setCredentials('admin', $password);
	if($g['debug']) {
		$cli->setDebug(1);
	}
	/* send our XMLRPC message and timeout after 250 seconds */
	$resp = $cli->send($msg, "250");
	if(!$resp) {
		$error = "A communications error occurred while attempting {$sync_logname} XMLRPC sync with {$url}:{$port}.";
		log_error($error);
		file_notice("sync_settings", $error, "{$sync_logname} Settings Sync", "");
	} elseif($resp->faultCode()) {
		$cli->setDebug(1);
		$resp = $cli->send($msg, "250");
		$error = "An error code was received while attempting {$sync_logname} XMLRPC sync with {$url}:{$port} - Code " . $resp->faultCode() . ": " . $resp->faultString();
		log_error($error);
		file_notice("sync_settings", $error, "{$sync_logname} Settings Sync", "");
	} else {
		log_error("{$sync_logname} XMLRPC sync successfully completed with {$url}:{$port}.");
	}

	/* tell package to reload our settings on the destination sync host. */
	$method = 'pfsense.exec_php';
	$execcmd  = "require_once('{$sync_include}');\n";
	$execcmd .= "{$sync_function}();\n";
	
	/* assemble xmlrpc payload */
	$params = array(
		XML_RPC_encode($password),
		XML_RPC_encode($execcmd)
	);

	log_error("{$sync_logname} XMLRPC reload data {$url}:{$port}.");
	$msg = new XML_RPC_Message($method, $params);
	$cli = new XML_RPC_Client('/xmlrpc.php', $url, $port);
	$cli->setCredentials('admin', $password);
	$resp = $cli->send($msg, "250");
	if(!$resp) {
		$error = "A communications error occurred while attempting {$sync_logname} XMLRPC sync with {$url}:{$port} (exec_php).";
		log_error($error);
		file_notice("sync_settings", $error, "{$sync_logname} Settings Reload", "");
	} elseif($resp->faultCode()) {
		$cli->setDebug(1);
		$resp = $cli->send($msg, "250");
		$error = "An error code was received while attempting {$sync_logname} XMLRPC sync with {$url}:{$port} - Code " . $resp->faultCode() . ": " . $resp->faultString();
		log_error($error);
		file_notice("sync_settings", $error, "{$sync_logname} Settings Sync", "");
	} else {
		log_error("{$sync_logname} XMLRPC reload data success with {$url}:{$port} (exec_php).");
	}
}
