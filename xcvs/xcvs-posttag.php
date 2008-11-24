#!/usr/bin/php
<?php
// $Id$
/**
 * @file
 * Insert branch/tag info into the Drupal database by processing
 * command line input and sending it to the Version Control API.
 *
 * Copyright 2005 by Kjartan Mannes ("Kjartan", http://drupal.org/user/2)
 * Copyright 2006, 2007 by Derek Wright ("dww", http://drupal.org/user/46549)
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

function xcvs_help($cli, $output_stream) {
  fwrite($output_stream, "Usage: $cli <config file> \$USER %t %b %o %p %{sTVv}\n\n");
}

function xcvs_exit($status, $lastlog, $summary) {
  @unlink($lastlog);
  @unlink($summary);
  exit($status);
}

function xcvs_get_item($action, $file_entry) {
  if ($file_entry) {
    list($path, $source_branch, $old, $new) = explode(",", $file_entry);

    return array(
      'type' => VERSIONCONTROL_ITEM_FILE,
      'path' => $path,
      'revision' => ($new != 'NONE') ? $new : $old,
      // currently unused... do we have a use for the source branch?
      //'source branch' => ($action == VERSIONCONTROL_ACTION_DELETED) ? NULL : $source_branch,
    );
  }
}

function xcvs_init($argc, $argv) {
  $this_file = array_shift($argv);   // argv[0]

  if ($argc < 7) {
    xcvs_help($this_file, STDERR);
    exit(3);
  }

  $config_file = array_shift($argv); // argv[1]

  // Load the configuration file and bootstrap Drupal.
  if (!file_exists($config_file)) {
    fwrite(STDERR, "Error: failed to load configuration file.\n");
    exit(4);
  }
  include_once $config_file;

  // Check temporary file storage.
  $tempdir = xcvs_get_temp_directory($xcvs['temp']);

  $username = array_shift($argv); // argv[2]
  $tag_name = array_shift($argv); // argv[3]
  $type = array_shift($argv);     // argv[4]
  $cvs_op = array_shift($argv);   // argv[5]
  $dir = array_shift($argv);      // argv[6]

  // Do a full Drupal bootstrap.
  xcvs_bootstrap($xcvs);

  // The commitinfo script wrote the lastlog file for us.
  // Its only contents is the name of the last directory that commitinfo
  // was invoked with, and that order is the same one as for loginfo.
  $lastlog = $tempdir .'/xcvs-lastlog.'. posix_getpgrp();
  $summary = $tempdir .'/xcvs-summary.'. posix_getpgrp();

  // Write the tagged/branched items to a temporary log file, one by one.
  while (!empty($argv)) {
    $filename = array_shift($argv);
    $source_branch = array_shift($argv);
    $source_branch = empty($source_branch) ? 'HEAD' : $source_branch;
    $old = array_shift($argv);
    $new = array_shift($argv);
    xcvs_log_add($summary, "/$dir/$filename,$source_branch,$old,$new\n", 'a');
  }

  // Once all logs in a multi-directory tagging/branching operation have been
  // gathered, the currently processed directory matches the last processed
  // directory that taginfo was invoked with, which means we've got all the
  // needed data in the summary file.
  if (xcvs_is_last_directory($lastlog, $dir)) {
    switch ($cvs_op) {
      case 'add':
        $action = VERSIONCONTROL_ACTION_ADDED;
        break;

      case 'mov':
        $action = VERSIONCONTROL_ACTION_MOVED;
        break;

      case 'del':
        // as $type == '?', we don't know if it's branches or tags,
        // so let go without asking the Version Control API
        // TODO: I think we can work around this by logging all branches and tags
        //       for each item in the database, and afterwards looking them up.
        $action = VERSIONCONTROL_ACTION_DELETED;
        xcvs_exit(0, $lastlog, $summary);

      default:
        fwrite(STDERR, "Error: unknown tag action.\n");
        xcvs_exit(5, $lastlog, $summary);
    }

    // Convert the previously written temporary log file
    // to Version Control API's item format.
    $fd = fopen($summary, "r");
    if ($fd === FALSE) {
      fwrite(STDERR, "Error: failed to open summary log at $summary.\n");
      xcvs_exit(6, $lastlog, $summary);
    }
    $operation_items = array();

    while (!feof($fd)) {
      $file_entry = trim(fgets($fd));
      $item = xcvs_get_item($action, $file_entry);
      if ($item) {
        $operation_items[$item['path']] = $item;
      }
    }
    fclose($fd);

    if (empty($operation_items)) {
      // if nothing is being tagged, we don't need to log anything.
      xcvs_exit(0, $lastlog, $summary);
    }

    if ($type == 'N') { // is a tag
      $type = VERSIONCONTROL_OPERATION_TAG;
    }
    else if ($type == 'T') { // is a branch
      $type = VERSIONCONTROL_OPERATION_BRANCH;
    }
    else { // does not happen at the moment (see 'del'), but just to make sure.
      exit(7);
    }

    $operation = array(
      'type' => $type,
      'date' => time(),
      'username' => $username,
      'repo_id' => $xcvs['repo_id'],
    );
    $label = array(
      'type' => $type,
      'name' => $tag_name,
      'action' => $action,
    );
    $operation['labels'] = array($label);

    versioncontrol_insert_operation($operation, $operation_items);

    // Clean up.
    xcvs_exit(0, $lastlog, $summary);
  }
  exit(0);
}

xcvs_init($argc, $argv);
