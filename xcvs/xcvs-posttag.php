#!/usr/bin/php
<?php
// $Id$
/**
 * @file
 * Provides access checking for 'cvs tag' commands.
 *
 * Copyright 2005 by Kjartan Mannes ("Kjartan", http://drupal.org/user/2)
 * Copyright 2006, 2007 by Derek Wright ("dww", http://drupal.org/user/46549)
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */
// TODO: move the "remove tags" restriction to Commit Restrictions
// TODO: implement the "don't remove release tags" restriction -
//       not in here, but rather in the release node integration

function xcvs_help($cli, $output_stream) {
  fwrite($output_stream, "Usage: $cli <config file> \$USER %t %b %o %p %{sTv}\n\n");
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
      'source branch' => ($action == VERSIONCONTROL_ACTION_DELETED) ? NULL : $source_branch,
    );
  }
}

function xcvs_init($argc, $argv) {
  fwrite(STDERR, print_r($argv, TRUE));

  $this_file = array_shift($argv);   // argv[0]

  if ($argc < 10) {
    xcvs_help($this_file, STDERR);
    exit(2);
  }

  $config_file = array_shift($argv); // argv[1]

  // Load the configuration file and bootstrap Drupal.
  if (!file_exists($config_file)) {
    fwrite(STDERR, "Error: failed to load configuration file.\n");
    exit(3);
  }
  include_once $config_file;

  $username = array_shift($argv); // argv[2]
  $tag = array_shift($argv);      // argv[3]
  $type = array_shift($argv);     // argv[4]
  $cvs_op = array_shift($argv);   // argv[5]
  $dir = array_shift($argv);      // argv[6]

  if ($xcvs['logs_combine']) {
    // Do a full Drupal bootstrap.
    xcvs_bootstrap($xcvs['drupal_path']);

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
      $old_revision = array_shift($argv);
      $old_revision = array_shift($argv);
      xcvs_log_add($summary, "/$dir/$filename,$source_branch,$old,$new\n", 'a');
    }

    // Once all logs in a multi-directory tagging/branching operation have been
    // gathered, the currently processed directory matches the last processed
    // directory that taginfo was invoked with, which means we've got all the
    // needed data in the summary file.
    if (xcvs_is_last_directory($lastlog, $commitdir)) {
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
          xcvs_exit(4, $lastlog, $summary);
      }

      // Convert the previously written temporary log file
      // to Version Control API's item format.
      $fd = fopen($summary, "r");
      if ($fd === FALSE) {
        fwrite(STDERR, "Error: failed to open summary log at $summary.\n");
        xcvs_exit(5, $lastlog, $summary);
      }
      $items = array();

      while (!feof($fd)) {
        $file_entry = trim(fgets($fd));
        $item = xcvs_get_item($action, $file_entry);
        if ($item) {
          $items[] = $item;
        }
      }
      fclose($fd);

      if (empty($items)) {
        // if nothing is being tagged, we don't need to log anything.
        xcvs_exit(0, $lastlog, $summary);
      }

      $tag_or_branch = array(
        'name' => $tag,
        'action' => $action,
        'date' => time(),
        'username' => $username,
        'repo_id' => $xcvs['repo_id'],
        'cvs_specific' => array(),
      );

      if ($type == 'N') { // is a tag
        versioncontrol_insert_tag_operation($tag_or_branch, $items);
      }
      else if ($type == 'T') { // is a branch
        versioncontrol_insert_branch_operation($tag_or_branch, $items);
      }
    }

    // Clean up.
    xcvs_exit(0, $lastlog, $summary);
  }
  exit(0);
}

xcvs_init($argc, $argv);
