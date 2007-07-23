#!/usr/bin/php
<?php
// $Id$
/**
 * @file
 * Configuration variables and bootstrapping code for all CVS hook scripts.
 *
 * Copyright 2005 by Kjartan Mannes ("Kjartan", http://drupal.org/user/2)
 * Copyright 2006, 2007 by Derek Wright ("dww", http://drupal.org/user/46549)
 * Copyright 2007 by Adam Light ("aclight", http://drupal.org/user/86358)
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

function xcvs_help($cli, $output_stream) {
  fwrite($output_stream, "Usage: $cli <config file> \$USER %p %{sVv}\n\n");
}

function xcvs_log_add($filename, $dir, $mode = 'w') {
  $fd = fopen($filename, $mode);
  fwrite($fd, $dir);
  fclose($fd);
}

function xcvs_is_last_directory($logfile, $dir) {
  if (file_exists($logfile)) {
    $fd = fopen($logfile, "r");
    $last = fgets($fd);
    fclose($fd);
    return $dir == $last ? 1 : 0;
  }
  return 1;
}

function xcvs_get_commit_action($file_entry) {
  if ($file_entry) {
    list($path, $old, $new) = explode(",", $file_entry);

    if ($old == 'dir') { // directories can only be added in CVS
      $action = array(
        'action' => VERSIONCONTROL_ACTION_ADDED,
        'current item' => array(
          'type' => VERSIONCONTROL_ITEM_DIRECTORY,
          'path' => $path,
          'revision' => NULL,
        ),
      );
      return array($path, $action);
    }

    $action = array();

    // If it's not a directory, it must be one of three possible file actions
    if ($old == 'NONE') {
      $action['action'] = VERSIONCONTROL_ACTION_REMOVED;
    }
    else if ($new == 'NONE') {
      $action['action'] = VERSIONCONTROL_ACTION_ADDED;
    }
    else {
      $action['action'] = VERSIONCONTROL_ACTION_MODIFIED;
    }

    if ($new != 'NONE') {
      $action['current item'] = array(
        'type' => VERSIONCONTROL_ITEM_FILE,
        'path' => $path,
        'revision' => $new,
      );
    }
    if ($old != 'NONE') {
      $action['source items'] = array(
        array(
          'type' => VERSIONCONTROL_ITEM_FILE,
          'path' => $path,
          'revision' => $old,
        ),
      );
    }

    return array($path, $action);
  }
}

/**
 * Go through the log message on the given input stream (yeah, I mean STDIN)
 * in order to extract branch and commit message.
 */
function xcvs_parse_log($input_stream) {
  $branch = 'HEAD';
  do {
    $line = trim(fgets($input_stream));
    if (preg_match("/^Tag:\s+(.+)$/", $line, $matches)) {
      $branch = trim($matches[1]);
    }
  } while ($line != "Log Message:");

  $message = "";
  while (!feof($input_stream)) {
    $message .= fgets($input_stream);
  }
  $message = trim($message);

  return array($branch, $message);
}

/**
 * Main function and starting point of this script:
 * Bootstrap Drupal, gather commit data and pass it on to Version Control API.
 */
function xcvs_init($argc, $argv) {
  $original_argv = $argv;
  $this_file = array_shift($argv);   // argv[0]
  $config_file = array_shift($argv); // argv[1]
  $username = array_shift($argv);    // argv[2]
  $commitdir = array_shift($argv);   // argv[3]

  if ($argc < 7) {
    xcvs_help($this_file, STDERR);
    exit(2);
  }

  // Load the configuration file and bootstrap Drupal.
  if (!file_exists($config_file)) {
    fwrite(STDERR, "Error: failed to load configuration file.\n");
    exit(3);
  }
  include_once $config_file;

  // Check temporary file storage.
  $tempdir = preg_replace('/\/+$/', '', $xcvs['temp']); // strip trailing slashes
  if (!(is_dir($tempdir) && is_writeable($tempdir))) {
    fwrite(STDERR, "Error: failed to access the temporary directory ($tempdir).\n");
    exit(4);
  }

  if ($xcvs['logs_combine']) {
    $lastlog = $tempdir .'/xcvs-lastlog.'. posix_getpgrp();
    $summary = $tempdir .'/xcvs-summary.'. posix_getpgrp();

    // Write the changed items to a temporary log file, one by one.
    if (!empty($argv)) {
      if ($argv[0] == '- New directory') {
        xcvs_log_add($summary, "/$commitdir,dir\n", 'a');
      }
      else {
        while (!empty($argv)) {
          $filename = array_shift($argv);
          $old = array_shift($argv);
          $new = array_shift($argv);
          xcvs_log_add($summary, "/$commitdir/$filename,$old,$new\n", 'a');
        }
      }
    }

    // Once all logs in a multi-directory commit have been gathered,
    // we get to this point with the right $commitdir. Or something like that.
    if (xcvs_is_last_directory($lastlog, $commitdir)) {
      // Convert the previously written temporary log file
      // to Version Control API's commit action format.
      $fd = fopen($summary, "r");
      if ($fd === FALSE) {
        fwrite(STDERR, "Error: failed to open summary log at $summary.\n");
        exit(5);
      }
      $commit_actions = array();

      while (!feof($fd)) {
        $file_entry = trim(fgets($fd));
        list($path, $action) = xcvs_get_commit_action($file_entry);
        if ($path) {
          $commit_actions[$path] = $action;
        }
      }
      fclose($fd);

      // TODO: Send out notification mails. Those should be moved to the
      //       real modules, ideally invoked by the
      //       versioncontrol_commit($op = 'insert') hook.

      // Integrate with the Drupal Version Control API.
      if ($xcvs['versioncontrol'] && !empty($commit_actions)) {
        // Find out how many lines have been added and removed for each file.
        foreach ($commit_actions as $path => $action) {
          if (!isset($action['current item'])) {
            continue;
          }

          $current_rev = $action['current item']['revision'];
          $trimmed_path = trim($path, '/');
          exec("/usr/bin/cvs -Qn -d $_ENV[CVSROOT] rlog -N -r$current_rev $trimmed_path", $result_lines);

          $matches = array();
          foreach ($result_lines as $line) {
            // 'date: 2004/08/20 07:51:22;  author: dries;  state: Exp;  lines: +2 -2'
            if (preg_match('/^date: .+;\s+lines: \+(\d+) -(\d+).*$/', $line, $matches)) {
              break;
            }
          }
          $commit_actions[$path]['cvs_specific'] = array(
            'lines_added' => (int) $matches[1],
            'lines_removed' => (int) $matches[2],
          );
        }

        // Get the remaining info from the commit log that we get from STDIN.
        list($branch_name, $message) = xcvs_parse_log(STDIN);

        // Map the username to the Drupal user id. We don't need to do this,
        // but by doing so we can avoid a few indirections that the
        // Version Control API would need to go through.
        $uid = db_result(db_query("SELECT uid FROM {versioncontrol_cvs_accounts}
                                   WHERE repo_id = '%d' AND username = '%s'",
                                  $xcvs['repo_id'], $username));
        if (!$uid) {
          $uid = 0;
        }

        // Get the branch id, and insert the branch into the database
        // if it doesn't exist yet.
        $branches = _versioncontrol_cvs_get_branches(array(
          'names' => array($branch_name),
        ));
        foreach ($branches as $id => $name) {
          $branch_id = $id;
          break; // we only asked for one branch name, so there's only one result
        }
        if (!isset($branch_id)) {
          $branch_id = _versioncontrol_cvs_insert_branch($branch_name);
        }

        // Prepare the data for passing it to Version Control API.
        $commit = array(
          'repo_id' => $xcvs['repo_id'],
          'date' => time(),
          'uid' => $uid,
          'username' => $username,
          'message' => $message,
          'revision' => NULL,
          'cvs_specific' => array(
            'branch_id' => $branch_id,
          ),
        );
        versioncontrol_insert_commit($commit, $commit_actions);

        // TODO: tag checking in project node integration?
      }
    }

    // Clean up
    @unlink($lastlog);
    @unlink($summary);
  }
  exit(0);
}

xcvs_init($argc, $argv);
