#!/usr/bin/php
<?php
// $Id$
/**
 * @file
 * Provides access checking for 'cvs commit' commands.
 *
 * Copyright 2005 by Kjartan Mannes ("Kjartan", http://drupal.org/user/2)
 * Copyright 2006, 2007 by Derek Wright ("dww", http://drupal.org/user/46549)
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

function xcvs_help($cli, $output_stream) {
  fwrite($output_stream, "Usage: $cli <config file> \$USER /%p %{s}\n\n");
}

function xcvs_get_commit_action($filename, $dir) {
  // Determine if the committed files were added, deleted or modified,
  // and construct an appropriate commit action entry for each file.
  // It checks for the existence of the file in the repository and/or
  // the working copy - see the commitinfo page of the CVS info manual
  // for a more detailed description of how this stuff works. Ugly, imho.

  $repository_path = $dir .'/'. $filename;

  $filepath_repository = $_ENV['CVSROOT'] . $dir .'/'. $filename .',v';
  $filepath_attic = $_ENV['CVSROOT']. $dir .'/Attic/'. $filename .',v';
  $exists_in_repository = (is_file($filepath_repository) || is_file($filepath_attic));

  $filepath_workingcopy = getcwd() .'/'. $filename;
  $exists_in_workingcopy = is_file($filepath_workingcopy);

  $action = array();

  if (!$exists_in_repository) {
    $action['action'] = VERSIONCONTROL_ACTION_ADDED;
  }
  else if (!$exists_in_workingcopy) {
    $action['action'] = VERSIONCONTROL_ACTION_DELETED;
  }
  else {
    $action['action'] = VERSIONCONTROL_ACTION_MODIFIED;
  }

  if ($exists_in_workingcopy) {
    $action['current item'] = array(
      'type' => VERSIONCONTROL_ITEM_FILE,
      'path' => $repository_path,
    );
  }
  if ($exists_in_repository) {
    $action['source items'] = array(
      array(
        'type' => VERSIONCONTROL_ITEM_FILE,
        'path' => $repository_path,
      ),
    );
  }

  return array($repository_path, $action);
}

function xcvs_init($argc, $argv) {
  $this_file = array_shift($argv);   // argv[0]

  if ($argc < 5) {
    xcvs_help($this_file, STDERR);
    exit(3);
  }

  $files = array_slice($argv, 4);

  $config_file = array_shift($argv); // argv[1]
  $username = array_shift($argv);    // argv[2]
  $dir = array_shift($argv);         // argv[3]
  $filenames = $argv; // the rest of the command line arguments

  // Load the configuration file and bootstrap Drupal.
  if (!file_exists($config_file)) {
    fwrite(STDERR, "Error: failed to load configuration file.\n");
    exit(4);
  }
  include_once $config_file;

  // Check temporary file storage.
  $tempdir = xcvs_get_temp_directory($xcvs['temp']);

  // Admins and other privileged users don't need to go through any checks.
  if (!in_array($username, $xcvs['allowed_users'])) {
    // Do a full Drupal bootstrap.
    xcvs_bootstrap($xcvs);

    // Construct a minimal commit array.
    $commit = array(
      'repo_id' => $xcvs['repo_id'],
      'username' => $username,
    );

    $commit_actions = array();
    foreach ($filenames as $filename) {
      list($path, $action) = xcvs_get_commit_action($filename, $dir);
      $commit_actions[$path] = $action;
    }

    // CVS doesn't tell us the branch at this point, so we need to pass NULL.
    $access = versioncontrol_has_commit_access($commit, $commit_actions, NULL);

    // Fail and print out error messages if commit access has been denied.
    if (!$access) {
      fwrite(STDERR, implode("\n\n", versioncontrol_get_access_errors()) ."\n\n");
      exit(5);
    }
  }
  // If we get as far as this, the commit may happen.

  // Remember this directory so that loginfo can combine commits
  // from different directories in one commit entry.
  $lastlog = $tempdir .'/xcvs-lastlog.'. posix_getpgrp();
  xcvs_log_add($lastlog, $dir);

  exit(0);
}

xcvs_init($argc, $argv);
