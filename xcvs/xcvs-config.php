<?php
// $Id$
/**
 * @file
 * Configuration variables and bootstrapping code for all CVS hook scripts.
 *
 * Copyright 2005 by Kjartan Mannes ("Kjartan", http://drupal.org/user/2)
 * Copyright 2006, 2007 by Derek Wright ("dww", http://drupal.org/user/46549)
 * Copyright 2007 by Adam Light ("aclight", http://drupal.org/user/86358)
 * Copyright 2007, 2008 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 * Copyright 2008 by Chad Phillips ("hunmonk", http://drupal.org/user/22079)
 */

// ------------------------------------------------------------
// Required customization
// ------------------------------------------------------------

// Base path of drupal directory (no trailing slash)
$xcvs['drupal_path'] = '/home/username/public_html';

// File location where to store temporary files.
$xcvs['temp'] = '/tmp';

// Drupal repository id that this installation of scripts is going to
// interact with. In order to find out the repository id, go to the
// "VCS repositories" administration page, then click on the "edit" link of
// the concerned repository, and notice the final number in the resulting URL.
$xcvs['repo_id'] = 1;


// ------------------------------------------------------------
// Optional customization
// ------------------------------------------------------------

// These users are always allowed full access, even if we can't
// connect to the DB. This optional list should contain the CVS
// usernames (not the Drupal username if they're different).
$xcvs['allowed_users'] = array();

// If you run a multisite installation, specify the directory
// name that your settings.php file resides in (ex: www.example.com)
// If you use the default settings.php file, leave this blank.
$xcvs['multisite_directory'] = '';


// ------------------------------------------------------------
// Shared code
// ------------------------------------------------------------

function xcvs_bootstrap($xcvs) {

  // add $drupal_path to current value of the PHP include_path
  set_include_path(get_include_path() . PATH_SEPARATOR . $xcvs['drupal_path']);

  $current_directory = getcwd();
  chdir($xcvs['drupal_path']);

  // bootstrap Drupal so we can use drupal functions to access the databases, etc.
  if (!file_exists('./includes/bootstrap.inc')) {
    fwrite(STDERR, "Error: failed to load Drupal's bootstrap.inc file.\n");
    exit(1);
  }

  // Set up the multisite directory if necessary.
  if ($xcvs['multisite_directory']) {
    $_SERVER['HTTP_HOST'] = $xcvs['multisite_directory'];
    // Set a dummy script name, so the multisite configuration
    // file search will always trigger.
    $_SERVER['SCRIPT_NAME'] = '/foo';
  }

  require_once './includes/bootstrap.inc';
  drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);

  chdir($current_directory);
}

function xcvs_get_temp_directory($temp_path) {
  $tempdir = preg_replace('/\/+$/', '', $temp_path); // strip trailing slashes
  if (!(is_dir($tempdir) && is_writeable($tempdir))) {
    fwrite(STDERR, "Error: failed to access the temporary directory ($tempdir).\n");
    exit(2);
  }
  return $tempdir;
}

function xcvs_log_add($filename, $dir, $mode = 'w') {
  $fd = fopen($filename, $mode);
  fwrite($fd, $dir);
  fclose($fd);
}

function xcvs_is_last_directory($logfile, $dir) {
  if (file_exists($logfile)) {
    $fd = fopen($logfile, 'r');
    $last = fgets($fd);
    fclose($fd);
    return $dir == $last ? TRUE : FALSE;
  }
  return TRUE;
}
