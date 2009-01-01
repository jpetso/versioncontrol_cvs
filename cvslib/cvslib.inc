<?php
// $Id$
/**
 * @file
 * A few generic functions for interfacing with CVS via command line.
 *
 * @note
 *    These functions can be run *without* Drupal.
 *
 * Copyright 2005 by Kjartan Mannes ("Kjartan", http://drupal.org/user/2)
 * Copyright 2006, 2007 by Derek Wright ("dww", http://drupal.org/user/46549)
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

// file_directory_temp() and t() are normally provided by Drupal, but as
// this file is supposed to be independent from Drupal code, here are
// fallback definitions for those functions.
if (!function_exists('file_directory_temp')) {
  function file_directory_temp() {
    return sys_get_temp_dir();
  }
}
if (!function_exists('t')) {
  function t($message) {
    return function_exists('cvslib_local_translate')
      ? cvslib_local_translate($message)
      : $message;
  }
}


/**
 * Perform a repository login if necessary, which is the case for pserver
 * access methods. Needs to be called once before the 'cvs' executable
 * is invoked (which is done automatically by the functions in this file).
 *
 * @param $repository_root
 *   The root directory of the CVS repository that you want to access.
 *   This function performs the necessary shell escapes, no need to do that
 *   beforehand.
 *
 * @return
 *   TRUE logging in was successful, or FALSE otherwise. If logging in did not
 *   succeed, any subsequent 'cvs' invocations will fail, so make sure you
 *   check the result value.
 */
function _cvslib_repository_login($repository_root) {
  if (strpos(':pserver:', $repository_root) !== FALSE) {
    $root = escapeshellcmd($repository_root);
    exec("cvs -d $root login", $output_lines, $return_status);
    if ($return_status != 0) {
      _cvslib_error_message(t('Failed to log in to CVS repository, could not fetch new repository logs.'));
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Append an appropriate output pipe to a $cmd array, which causes STDERR
 * to be written to a random file.
 *
 * @return
 *   An array with the temporary files that will be created when $cmd
 *   is executed. In its current form, the return array only contains
 *   the filename for STDERR output as 'stderr' array element.
 */
function _cvslib_add_output_pipes(&$cmd) {
  $tempdir = file_directory_temp();
  $tempfiles = array();

  // Always use a temporary file unless the site specifically wants us not to.
  if (!function_exists('cvslib_local_use_file') || cvslib_local_use_file() == TRUE) {
    $tempfiles['stdout'] = $tempdir .'/drupal_versioncontrol_cvs.stdout.'. mt_rand() .'.txt';
    $cmd[] = '> '. $tempfiles['stdout'];
  }
  $tempfiles['stderr'] = $tempdir .'/drupal_versioncontrol_cvs.stderr.'. mt_rand() .'.txt';
  $cmd[] = '2> '. $tempfiles['stderr'];
  return $tempfiles;
}

/**
 * Delete temporary files that have been created by a command which included
 * output pipes from _cvslib_add_output_pipes().
 */
function _cvslib_delete_temporary_files($tempfiles) {
  if (isset($tempfiles['stdout'])) {
    @unlink($tempfiles['stdout']);
  }
  @unlink($tempfiles['stderr']);
}

/**
 * Read the STDERR output for a command that was executed.
 * The output must have been written to a temporary file which was given
 * by _cvslib_add_output_pipes(). The temporary file is deleted after it
 * has been read. After calling the function, the error message can be
 * retrieved by calling cvslib_last_error_message() or discarded by calling
 * cvslib_unset_error_message().
 */
function _cvslib_set_error_message($tempfiles) {
  _cvslib_error_message(file_get_contents($tempfiles['stderr']));
  @unlink($tempfiles['stderr']);
}

/**
 * Retrieve the STDERR output from the last invocation of 'cvs' that exited
 * with a non-zero status code. After fetching the error message, it will be
 * unset again until a subsequent 'cvs' invocation fails as well. If no message
 * is set, this function returns NULL.
 *
 * For better security, it is advisable to run the returned error message
 * through check_plain() or similar string checker functions.
 */
function cvslib_last_error_message() {
  $message = _cvslib_error_message();
  _cvslib_error_message(FALSE);
  return $message;
}

/**
 * Write or retrieve an error message, stored in a static variable.
 *
 * @param $info
 *   NULL to retrieve the message, FALSE to unset it, or a string containing
 *   the new message to remember it for later retrieval.
 */
function _cvslib_error_message($message = NULL) {
  static $error_message = NULL;

  if (!isset($message)) {
    return $error_message;
  }
  else {
    $error_message = ($message === FALSE) ? NULL : $message;
    return $error_message;
  }
}

/**
 * Execute a command (given as array of command line parts), check the
 * return code (setting an error message if it's non-zero), and prepare
 * the output text for further processing.
 *
 * @return
 *   Boolean FALSE (to be checked with "===" or "!==") if the return code was
 *   non-zero. Otherwise, either an array of output lines or a read-opened
 *   file handle for the output - which one you get depends on whether or not
 *   an output file was specified for the stdout pipe.
 */
function _cvslib_exec($cmd, $tempfiles) {
  $cmd = implode(' ', $cmd);

  $return_code = 0;
  exec($cmd, $entries, $return_code);
  watchdog('special', $cmd);

  if ($return_code != 0) {
    _cvslib_set_error_message($tempfiles);
    return FALSE;
  }

  if (isset($tempfiles['stdout'])) {
    $output = fopen($tempfiles['stdout'], 'r');
    if ($output === FALSE) {
      return FALSE;
    }
  }
  else {
    reset($output); // reset the array pointer, so that we can use next()
  }
  return $output;
}

/**
 * Prepare a repository item path for usage as CVS command line argument.
 */
function _cvslib_fix_path($path) {
  if ($path == '/') {
    $path = '.';
  }
  if ($path[0] == '/') {
    $path = substr($path, 1);
  }
  return $path;
}


/**
 * Retrieve and parse the logs of one or more items (files or directories)
 * into a list of file revision objects that they can be processed more easily.
 *
 * @param $repository_root
 *   The root directory of the CVS repository that you want to access.
 *   This function performs the necessary shell escapes, no need to do that
 *   beforehand.
 * @param $item_path
 *   A string containing an item path relative to the repository root.
 *   This function will retrieve the file revisions for each of the
 *   corresponding items, or for all of their descendant files if the path
 *   denotes a directory. Both paths starting with a slash ("/") and paths
 *   omitting it are accepted as input.
 *
 * @return
 *   An array that will be filled with a simple, flat list of
 *   file revision objects. Each object has the following properties:
 *
 *   [todo]
 *
 *   If any errors occurred for any of the given items, boolean FALSE will be
 *   returned - remember to check the return value with "===" or "!==".
 */
/*function cvslib_ls($repository_root, $item_path) {
  if (!_cvslib_repository_login($repository_root)) {
    return FALSE;
  }
  $cmd = array(
    'cvs',
    '-qnf', // standard global arguments: quiet, no disk changes, no ~/.cvsrc file
    '-d '. escapeshellcmd($repository_root),
    'rls',
    '-lRd',
    _cvslib_fix_path($path),
  );
  $tempfiles = _cvslib_add_output_pipes($cmd);
  $entries = _cvslib_exec($cmd, $tempfiles);

  if ($entries === FALSE) {
    return FALSE;
  }

  // Parse the info from the raw output.
  $file_revisions = _cvslib_parse_ls($repository_root, $entries);

  // Close the stdout file and delete both stdout and stderr output.
  if (is_resource($entries)) {
    fclose($entries);
  }
  _cvslib_delete_temporary_files($tempfiles);

  return $file_revisions;
}

function _cvslib_parse_ls($repository_root, $entries) {
  // If the log was retrieved by taking the return value of exec(), we've
  // got and array and navigate it via next(). If we stored the log in a
  // temporary file, $entries is a file handle that we need to fgets() instead.
  $next = is_array($entries) ? 'next' : 'fgets';

  $file_revisions = array();

  /// TODO: implement

  return $file_revisions;
}*/


/**
 * Retrieve and parse the logs of one or more items (files or directories)
 * into a list of file revision objects that they can be processed more easily.
 * This function is equivalent to
 * "cvs rls -qnf -d $repository_root rlog -S $item_paths".
 *
 * @param $repository_root
 *   The root directory of the CVS repository that you want to access.
 *   This function performs the necessary shell escapes, no need to do that
 *   beforehand.
 * @param $item_paths
 *   An array of item paths relative to the repository root, or a string
 *   containing a single path. This function will retrieve the file revisions
 *   for each of the corresponding items, or for all of their descendant files
 *   if the path denotes a directory. Both paths starting with a slash ("/")
 *   and paths omitting it are accepted as input.
 * @param $constraints
 *   An array of filter constraints that specify which file revisions should
 *   be retrieved. The following array elements are accepted (all are optional):
 *
 *   - 'date_lower': A Unix timestamp. If given, no file revisions will be
 *        retrieved that were performed earlier than this lower bound.
 *   - 'date_upper': A Unix timestamp. If given, no file revisions will be
 *        retrieved that were performed later than this upper bound.
 *
 * @return
 *   An array that will be filled with a simple, flat list of
 *   file revision objects. Each object has the following properties:
 *
 *   - path: The absolute path in the repository, starting with '/'.
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
 *
 *   If any errors occurred for any of the given items, boolean FALSE will be
 *   returned - remember to check the return value with "===" or "!==".
 */
function cvslib_log($repository_root, $item_paths = '.', $constraints = array()) {
  if (!_cvslib_repository_login($repository_root)) {
    return FALSE;
  }
  if (is_string($item_paths)) {
    $item_paths = array($item_paths);
  }
  if (empty($item_paths)) {
    $item_paths[] = '.';
  }
  $root = escapeshellcmd($repository_root);
  $filter_args = array();

  // Prepare the date filter argument.
  if (isset($constraints['date_lower']) && !isset($constraints['date_upper'])) {
    $filter_args[] = '-d ">=@'. $constraints['date_lower'] .'"';
  }
  else if (!isset($constraints['date_lower']) && isset($constraints['date_upper'])) {
    $filter_args[] = '-d "<=@'. $constraints['date_upper'] .'"';
  }
  else if (isset($constraints['date_lower']) && isset($constraints['date_upper'])) {
    $filter_args[] = '-d "@'. $constraints['date_lower']
      .'<=@'. $constraints['date_upper'] .'"';
  }

  // Call CVS in order to get the raw logs.
  $cmd = array(
    'cvs',
    '-qnf', // standard global arguments: quiet, no disk changes, no ~/.cvsrc file
    '-d '. $root,
    'rlog',
    '-S', // "Do not print name/header if no revisions selected."
  );
  foreach ($filter_args as $arg) {
    $cmd[] = $arg;
  }
  foreach ($item_paths as $path) {
    $cmd[] = _cvslib_fix_path($path);
  }
  $cmd[] = escapeshellcmd($path);
  $tempfiles = _cvslib_add_output_pipes($cmd);
  $logs = _cvslib_exec($cmd, $tempfiles);

  if ($logs === FALSE) {
    return FALSE;
  }

  // Parse the info from the raw output.
  $file_revisions = _cvslib_parse_log($repository_root, $logs, $file_revisions);

  // Close the stdout file and delete both stdout and stderr output.
  if (is_resource($logs)) {
    fclose($logs);
  }
  _cvslib_delete_temporary_files($tempfiles);

  return $file_revisions;
}

/**
 * Get the part of a string that is right to the first colon,
 * trimming spaces on both input and result text.
 */
function _cvslib_explode($text, $delim = ':') {
  $parts = explode($delim, $text, 2);
  return trim($parts[1]);
}

/**
 * Parse the logs into a list of file revision objects, so that they
 * can be processed more easily.
 *
 * @param $repository_root
 *   The unescaped root directory of the CVS repository.
 * @param $logs
 *   Either an array containing all the output lines (if the output was
 *   directly read by exec()) or a file handle of the temporary file
 *   that the output was written to.
 *
 * @return
 *   An simple, flat list of file revision objects (see cvslib_log() for the
 *   format description).
 */
function _cvslib_parse_log($repository_root, &$logs) {
  // If the log was retrieved by taking the return value of exec(), we've
  // got and array and navigate it via next(). If we stored the log in a
  // temporary file, $logs is a file handle that we need to fgets() instead.
  $next = is_array($logs) ? 'next' : 'fgets';

  // Remove prefixes like ":pserver:" from the repository root.
  $root_path = preg_replace('|[^/]*(/.+)$|', '\1', $repository_root);

  $file_revisions = array();

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
    $file->number_revisions = _cvslib_explode($parts[1]); // "2" in the above example

    // Skip until "description" (which should be the next line anyways, usually)
    while (trim($next($logs)) != "description:") {
    }
    $separator = $next($logs); // like, "----------------------------"

    for ($i = 0; $i < $file->number_revisions; $i++) {
      $file_revision = new stdClass();
      $file_revision->path = $file->path;

      $parts = explode(' ', $next($logs)); // that line is like "revision 1.9"
      $file_revision->revision = trim($parts[1]);

      // Example line (commitid is only in more recent versions of CVS):
      // date: 2007-10-02 20:44:15 +0100;  author: jakob;  state: Exp;  lines: +2 -1;  commitid: vaXgz7afKtx3m3As;
      $line = $next($logs);
      $parts = explode(';', $line);
      $file_revision->date = strtotime(_cvslib_explode($parts[0]));
      $file_revision->username = _cvslib_explode($parts[1]);

      // "state" is "Exp" or "dead" (in case no low-level modifications
      // involving 'rcs' were performed), so store this as boolean.
      $file_revision->dead = (_cvslib_explode($parts[2]) == 'dead');

      // "lines: (...)" from the above example line only appears
      // for revisions other than 1.1.
      $lines = array();
      if ($file_revision->revision !== '1.1' && $file_revision->dead == FALSE) {
        $lines = explode(' ', _cvslib_explode($parts[3]));
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

  return $file_revisions;
}

