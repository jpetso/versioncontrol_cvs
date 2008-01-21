<?php
// $Id$
/**
 * @file
 * CVS backend for Version Control API - Provides CVS commit information and
 * account management as a pluggable backend.
 *
 * This file provides functionality to parse the output of 'cvs rlog'
 * and transform it into Version Control API commits.
 *
 * Copyright 2005 by Kjartan Mannes ("Kjartan", http://drupal.org/user/2)
 * Copyright 2006, 2007 by Derek Wright ("dww", http://drupal.org/user/46549)
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

/**
 * Actually update the repository by fetching commits and other stuff
 * directly from the repository, invoking the cvs executable.
 *
 * @return
 *   TRUE if the logs were updated, or FALSE if fetching and updating the logs
 *   failed for whatever reason.
 */
function _versioncontrol_cvs_log_update_repository(&$repository) {
  $root = escapeshellcmd($repository['root']);

  // If the repository is a remote one, log in before performing any fetches
  if (strpos(':pserver:', $root) !== FALSE) {
    exec("cvs -d $root login", $output_lines, $return_status);
    if ($return_status) {
      drupal_set_message(t('Failed to log in to CVS repository, could not fetch new repository logs.'), 'error');
      return FALSE;
    }
  }

  // Prepare the "since when" time argument for CVS, and remember the
  // current time for the "updated" value that is stored later on.
  $date = ($repository['cvs_specific']['updated'] > 0)
          ? '-d ">='. gmdate('Y-m-d H:i:s', $repository['cvs_specific']['updated']) .' UTC"'
          : '';
  $date_updated = time();

  // Determine which paths will be queried for logs.
  $subdirs = $repository['cvs_specific']['modules'];

  // If no paths are given, fetch logs for the whole repository.
  if (empty($subdirs)) {
    $subdirs = array('.');
  }
  $file_revisions = array();

  // Call CVS in order to get the raw logs.
  foreach ($subdirs as $subdir) {
    $subdir = escapeshellcmd($subdir);

    if (variable_get('versioncontrol_cvs_log_use_file', 1)) {
      $temp_dir = variable_get('file_directory_temp', (PHP_OS == 'WINNT' ? 'c:\\windows\\temp' : '/tmp'));
      $temp_file = $temp_dir .'/cvs-'. rand();
      exec("cvs -qnf -d $root rlog -S $date $subdir > $temp_file");
      $logs = fopen($temp_file, 'r');
    }
    else {
      exec("cvs -qnf -d $root rlog -S $date $subdir", $logs);
      reset($logs); // reset the array pointer, so that we can use next()
    }
    watchdog('special', "cvs -qnf -d $root rlog -S $date $subdir");

    // Parse the info from the raw output.
    _versioncontrol_cvs_log_parse($repository, $logs, $file_revisions);

    if (variable_get('versioncontrol_cvs_log_use_file', 1)) {
      fclose($logs);
      unlink($temp_file);
    }
    $updated = TRUE;
  }

  if ($updated) {
    // Having retrieved the file revisions, insert those into the database
    // as Version Control API commits.
    _versioncontrol_cvs_log_process($repository, $file_revisions);
    $repository['cvs_specific']['updated'] = $date_updated;

    // Everything's done, remember the time when we updated the log (= now).
    db_query('UPDATE {versioncontrol_cvs_repositories}
              SET updated = %d WHERE repo_id = %d',
              $repository['cvs_specific']['updated'], $repository['repo_id']);
  }
  return TRUE;
}

/**
 * Get the part of a string that is right to the first colon,
 * trimming spaces on both input and result text.
 */
function _versioncontrol_cvs_explode($text, $delim = ':') {
  $parts = explode($delim, $text, 2);
  return trim($parts[1]);
}

/**
 * Parse the logs into a list of file revision objects, so that they
 * can be processed more easily.
 *
 * @param $repository
 *   The repository array, as given by the Version Control API.
 * @param $logs
 *   Either an array containing all the output lines (if the output was
 *   directly read by exec()) or a file handle of the temporary file
 *   that the output was written to.
 * @param $file_revisions
 *   An array that will be filled with a simple, flat list of
 *   file revision objects. Each object has the following properties:
 *
 *   - revision: The revision number (a string, e.g. '1.1' or '1.59.2.3').
 *   - date: The time of the revision, as Unix timestamp.
 *   - username: The CVS username of the committer.
 *   - dead: TRUE if the file revision is in the "dead" (deleted) state,
 *       or FALSE if it currently exists in the repository.
 *   - lines_added: An integer that specifies how many lines have been added
 *       in this revision.
 *   - lines_removed: An integer that specifies how many lines have been added
 *       in this revision.
 *   - commitid: Optional property, may exist in more recent versions of CVS.
 *       (It seems to have been introduced in 2005 or something.) If given,
 *       this is a string which is the same for all file revisions in a commit.
 *   - message: The commit message (a string with possible line breaks).
 *   - branch: The branch that this file revision was committed to,
 *       as string containing the name of the branch.
 */
function _versioncontrol_cvs_log_parse($repository, &$logs, &$file_revisions) {
  // If the log was retrieved by taking the return value of exec(), we've
  // got and array and navigate it via next(). If we stored the log in a
  // temporary file, $logs is a file handle that we need to fgets() instead.
  $next = is_array($logs) ? 'next' : 'fgets';

  // Remove prefixes like ":pserver:" from the repository root.
  $root_path = preg_replace('|[^/]*(/.+)$|', '\1', $repository['root']);

  while (($line = $next($logs)) !== FALSE) {
    $matches_found = preg_match('/^RCS file: (.+)$/', $line, $matches);
    if (!$matches_found) {
      continue;
    }
    $file = new stdClass();

    // Remove the root path and the trailing ",v".
    $file->path = trim(preg_replace("@^$root_path(.*)(,v)$@", '\1', $matches[1]));
    // Remove a possible "Attic/" directory that exists if the file
    // is currently in a "dead" state.
    $file->path = preg_replace('@^(.*/)Attic/(.*)$@', '\1\2', $file->path);

    $next($logs); // head - not used
    $next($logs); // branch - not used
    $next($logs); // locks - not used
    $next($logs); // access - not used

    // Retrieve branches and tags ("symbolic names" is the common term here).
    if (trim($next($logs)) == 'symbolic names:') {
      $file->branches = array();
      $file->tags = array();

      while (TRUE) {
        $line = $next($logs);

        if (preg_match('/^keyword substitution: (.*)$/', $line, $matches)) {
          // $matches[1] could be stored as $file->keyword, but is not used.
          break; // no branches and tags anymore, go on with the next steps
        }
        $parts = explode(':', trim($line)); // e.g. "DRUPAL-5--2-0: 1.4"

        // If the revision ends with "0.N", we know this is a branch.
        if (preg_match('/\.0\.\d+$/', trim($parts[1]))) {
          // When saving the revision number for branches, we need to
          // move the final N into the place of the '.0' when we
          // save it so that we can compare revisions numbers against
          // this value and match them to the right branch.
          $branch_prefix = preg_replace('/^(.+?)\.\d+(\.\d+)$/', '\1\2', trim($parts[1]));
          $file->branches[$branch_prefix] = trim($parts[0]);
        }
        else {
          // There's no magic for revision numbers on non-branch tags.
          // However, since multiple tags can point to the same
          // revision, here we want to key on the tag name.
          $file->tags[trim($parts[0])] = trim($parts[1]);
        }
      }
    }

    // Next line looks like "total revisions: 4;     selected revisions: 2"
    $parts = explode(';', $next($logs));
    $file->number_revisions = _versioncontrol_cvs_explode($parts[1]); // "2" in the above example

    // Skip until "description" (which should be the next line anyways, usually)
    while (trim($next($logs)) != "description:") {
    }
    $separator = $next($logs); // like, "----------------------------"

    for ($i = 0; $i < $file->number_revisions; $i++) {
      $file_revision = new StdClass();
      $file_revision->path = $file->path;

      $parts = explode(' ', $next($logs)); // that line is like "revision 1.9"
      $file_revision->revision = trim($parts[1]);

      // Example line (commitid is only in more recent versions of CVS):
      // date: 2007-10-02 20:44:15 +0100;  author: jakob;  state: Exp;  lines: +2 -1;  commitid: vaXgz7afKtx3m3As;
      $line = $next($logs);
      $parts = explode(';', $line);
      $file_revision->date = strtotime(_versioncontrol_cvs_explode($parts[0]));
      $file_revision->username = _versioncontrol_cvs_explode($parts[1]);

      // "state" is "Exp" or "dead" (in case no low-level modifications
      // involving 'rcs' were performed), so store this as boolean.
      $file_revision->dead = (_versioncontrol_cvs_explode($parts[2]) == 'dead');

      // "lines: (...)" from the above example line only appears
      // for revisions other than 1.1.
      $lines = array();
      if ($file_revision->revision !== '1.1' && $file_revision->dead == FALSE) {
        $lines = explode(' ', _versioncontrol_cvs_explode($parts[3]));
      }
      $file_revision->lines_added = empty($lines) ? 0 : abs($lines[0]);
      $file_revision->lines_removed = empty($lines) ? 0 : abs($lines[1]);

      // commitid is only in more recent versions of CVS -
      // use it if it's given, or fall back to single-file commits if not.
      if (preg_match('/^.+;\s+commitid: ([^;]+).*$/', $line, $matches)) {
        $file_revision->commitid = $matches[1];
      }

      // The next line is either "branches: (...)"
      // or the first line of the commit message.
      $line = $next($logs);
      $message = '';

      if (substr($line, 0, 9) != 'branches:') {
        // Not sure if $next() always includes linebreaks or not -> trim.
        $message = trim($line) ."\n";
      }

      // After that, we have either more message lines or the end of the message.
      while (($line = $next($logs)) != $separator && trim($line) != "=============================================================================") {
        $message .= "$line\n";
      }
      $file_revision->message = trim($message);

      // Retrieve the branch of this revision.
      $parts = explode('.', $file_revision->revision);

      if (empty($file->branches) || count($parts) <= 2) {
        $file_revision->branch = 'HEAD';
      }
      else {
        // Let's say we start with "1.59.2.7".
        array_pop($parts); // "1.59.2" is the only possible branch prefix
        $branch_prefix = implode('.', $parts);

        if (isset($file->branches[$branch_prefix])) {
          // Get the name of the branch that maps to this branch prefix.
          $file_revision->branch = $file->branches[$branch_prefix];
        }
        else { // should not happen, but who knows... maybe with deleted branches?
          $file_revision->branch = ''; // "branch is unknown"
        }
      }

      $file_revisions[] = $file_revision;
    } // loop to the next revision of this file
  } // loop to the next file
}

/**
 * Update the database by processing and inserting the previously retrieved
 * file revision objects.
 *
 * @param $repository
 *   The repository array, as given by the Version Control API.
 * @param $file_revisions
 *   A simple, flat list of file revision objects - the combined set of
 *   return values from _versioncontrol_cvs_log_parse().
 */
function _versioncontrol_cvs_log_process($repository, $file_revisions) {
  $commit_actions_by_commitid = array();
  $commit_actions_by_user = array();

  foreach ($file_revisions as $file_revision) {
    // Don't insert the same revision twice.
    $count = db_result(db_query(
      "SELECT COUNT(*)
       FROM {versioncontrol_cvs_item_revisions} ir
        INNER JOIN {versioncontrol_operations} op
         ON ir.vc_op_id = op.vc_op_id
       WHERE op.repo_id = %d AND op.type = %d
        AND ir.path = '%s' AND ir.revision = '%s'",
      $repository['repo_id'], VERSIONCONTROL_OPERATION_COMMIT,
      $file_revision->path, $file_revision->revision
    ));
    if ($count > 0) {
      continue;
    }

    // We might only pick one of those (depending if the file
    // has been added, modified or deleted) but let's add both
    // current and source items for now.
    $commit_action = array(
      'action' => VERSIONCONTROL_ACTION_MODIFIED, // default, might be changed
      'current item' => array(
        'type' => VERSIONCONTROL_ITEM_FILE,
        'path' => $file_revision->path,
        'revision' => $file_revision->revision,
      ),
      'source items' => array(
        array(
          'type' => VERSIONCONTROL_ITEM_FILE,
          'path' => $file_revision->path,
          'revision' => versioncontrol_cvs_get_previous_revision_number($file_revision->revision),
        ),
      ),
      'cvs_specific' => array(
        'file_revision' => $file_revision, // temporary
        'lines_added' => $file_revision->lines_added,
        'lines_removed' => $file_revision->lines_removed,
      ),
    );

    if ($file_revision->dead) {
      $commit_action['action'] = VERSIONCONTROL_ACTION_DELETED;
      unset($commit_action['current item']);
    }
    else {
      if ($file_revision->revision === '1.1') {
        $commit_action['action'] = VERSIONCONTROL_ACTION_ADDED;
        unset($commit_action['source items']);
      }
    }

    if (isset($file_revision->commitid)) {
      $commit_actions_by_commitid[$file_revision->commitid][$file_revision->path] = $commit_action;
    }
    else {
      $commit_actions_by_user[$file_revision->username]
        [$file_revision->date][$file_revision->path] = $commit_action;
    }
  }

  $commits = array();

  // Part one: revisions with commitid - these are cool & easy.
  foreach ($commit_actions_by_commitid as $commitid => $commit_actions) {
    _versioncontrol_cvs_log_construct_commit($repository, $commit_actions, $commits);
  }

  // Part two: revisions without commitid - need to apply heuristics
  // in order to get whole commits instead of separate file-by-file stuff.
  foreach ($commit_actions_by_user as $username => $commit_actions_by_date) {
    // Iterating through the date sorted array is a bit complicated
    // as we need to delete file revision elements that are determined to be
    // in the same commit as the reference action.
    while ($date = key($commit_actions_by_date)) {
      while ($path = key($commit_actions_by_date[$date])) {
        $reference_action = array_shift($commit_actions_by_date[$date]);

        $commit_actions = _versioncontrol_cvs_log_group_actions(
          $commit_actions_by_date, $reference_action
        );
        _versioncontrol_cvs_log_construct_commit($repository, $commit_actions, $commits);
      }
      unset($commit_actions_by_date[$date]); // Done with this date, next one.
      reset($commit_actions_by_date); // Set the array pointer to the start.
    }
  }

  // Ok, we've got all commits gathered and in a nice array with
  // the commit date as key. So the only thing that's left is to sort them
  // and then send each commit to the API function for inserting into the db.
  ksort($commits);
  foreach ($commits as $date => $date_commits) {
    foreach ($date_commits as $commit_info) {
      _versioncontrol_cvs_fix_commit_actions($commit_info->commit, $commit_info->commit_actions);
      versioncontrol_insert_commit($commit_info->commit, $commit_info->commit_actions);
    }
  }
}

/**
 * Extract (and delete) actions from the given $commit_actions_by_date array
 * that belong to the same commit as the $reference_action.
 * This function is what provides heuristics for grouping file revisions
 * (that lack a commitid) together into one commit.
 *
 * @return
 *   One or more commit actions grouped into a commit actions array,
 *   complete with file paths as keys as required by the Version Control API.
 */
function _versioncontrol_cvs_log_group_actions(&$commit_actions_by_date, $reference_action) {
  $file_revision = $reference_action['cvs_specific']['file_revision'];

  $commit_actions = array();
  $commit_actions[$file_revision->path] = $reference_action;

  // Try all file revisions in the near future (next 30 seconds) to see if
  // they belong to the same commit or not. If they do, extract and delete.
  // Commits that take longer than half a minute are unlikely enough to be
  // disregarded here.
  for ($date = $file_revision->date; $date < ($file_revision->date + 30); $date++) {
    if (!isset($commit_actions_by_date[$date])) {
      continue;
    }

    foreach ($commit_actions_by_date[$date] as $path => $current_action) {
      $current_file_revision = $current_action['cvs_specific']['file_revision'];

      // Check for message and branch to be similar. We know that the username
      // is similar because of we sorted by that one, and the date is
      // near enough to be regarded as roughly the same time.
      if ($current_file_revision->message == $file_revision->message
          && $current_file_revision->branch == $file_revision->branch) {
        // So, sure enough, we have a file from the same commit here.
        $commit_actions[$path] = $current_action;
        unset($commit_actions_by_date[$date][$path]); // Don't process this revision twice.
      }
    }
  }
  return $commit_actions;
}

/**
 * Use the additional file revision information that has been stored
 * in each commit action array in order to assemble the associated commit.
 * That commit information is then stored as a list item in the given
 * $commits array as an object with 'commit' and 'commit_actions' properties.
 */
function _versioncontrol_cvs_log_construct_commit($repository, $commit_actions, &$commits) {
  $date = 0;

  // Get any of those commit properties, apart from the date (which may
  // vary in large commits) they should all be the same anyways.
  foreach ($commit_actions as $path => $commit_action) {
    $file_revision = $commit_action['cvs_specific']['file_revision'];
    unset($commit_actions[$path]['cvs_specific']['file_revision']);

    if ($file_revision->date > $date) {
      $date = $file_revision->date;
    }
    $username = $file_revision->username;
    $message = $file_revision->message;
    $branch_name = $file_revision->branch;
  }

  // Get the branch id, and insert the branch into the database
  // if it doesn't exist yet.
  $branch_id = versioncontrol_ensure_branch($branch_name, $repository['repo_id']);

  // Yay, we have all commit actions and all information. Ready to go!
  $commit = array(
    'repo_id' => $repository['repo_id'],
    'date' => $date,
    'username' => $username,
    'message' => $message,
    'revision' => '',
    'cvs_specific' => array(
      'branch_id' => $branch_id,
    ),
  );

  $commit_info = new StdClass();
  $commit_info->commit = $commit;
  $commit_info->commit_actions = $commit_actions;
  $commits[$date][] = $commit_info;
}
