<?php
// $Id$
/**
 * @file
 * Configuration variables and bootstrapping code for all CVS hook scripts.
 *
 * Copyright 2005 by Kjartan ("Kjartan", http://drupal.org/user/2)
 * Copyright 2006, 2007 by Derek Wright ("dww", http://drupal.org/user/46549)
 * Copyright 2007 by Adam Light ("aclight", http://drupal.org/user/86358)
 * Copyright 2007 by Jakob Petsovits <jpetso@gmx.at>
 */

// ------------------------------------------------------------
// Required customization
// ------------------------------------------------------------

// Base path of drupal directory (no trailing slash)
$xcvs['drupal_path'] = '/home/username/public_html';

// File location where to store temporary files.
$xcvs['temp'] = "/tmp";

// e-mail address where all cvs commit log emails should be sent. If not
// specified, e-mails are only sent if $xcvs["logs_mailto_maintainers"] is TRUE.
$xcvs['logs_mailto'] = 'user@example.com';

// Should commit log emais be sent to project owners and maintainers
// TODO:  write code to make this happen
$xcvs['logs_mailto_maintainers'] = TRUE;

// From header to use for commit log emails.
$xcvs['logs_from'] = 'admin@example.com';

// Drupal repository id that this installation of scripts is going to
// interact with. If you only have one repository, leave this as '1'.
// If you have multiple repositories installed via the cvs.module, you
// can find the appropriate value at the "admin/cvs/repositories" page
// on your site. Click on the "edit" link, and notice the final number
// in the resulting URL.
$xcvs['repo_id'] = 1;


// ------------------------------------------------------------
// Optional customization
// ------------------------------------------------------------

// Should these scripts update the Drupal database with commit logs
// and information to provide Version Control API integration?
$xcvs['versioncontrol'] = TRUE;

// Combine the commit log messages for a multidir commit into one mail.
$xcvs["logs_combine"] = TRUE;


// ------------------------------------------------------------
// Internal code
// ------------------------------------------------------------

function bootstrap($drupal_path) {
  // add $drupal_path to current value of the PHP include_path
  set_include_path(get_include_path() . PATH_SEPARATOR . $drupal_path);

  $current_directory = getcwd();
  chdir($drupal_path);

  // bootstrap Drupal so we can use drupal functions to access the databases, etc.
  if (!file_exists('./includes/bootstrap.inc')) {
    fwrite(STDERR, "Error: failed to load Drupal's bootstrap.inc file.\n");
    exit(1);
  }
  require_once './includes/bootstrap.inc';
  drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);

  chdir($current_directory);
}

// First thing to do: bootstrap Drupal.
bootstrap($xcvs['drupal_path']);

// $xcvs has to be made global so the xcvs-taginfo.php script works properly.
global $xcvs_global;
$xcvs_global = $xcvs;
