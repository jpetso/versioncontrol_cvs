#!/usr/bin/php
<?php
// $Id$
/**
 * @file
 * Configuration variables and bootstrapping code for all CVS hook scripts.
 *
 * Copyright 2005 by Kjartan ("Kjartan", http://drupal.org/user/2)
 * Copyright 2006, 2007 by Derek Wright ("dww", http://drupal.org/user/46549)
 * Copyright 2007 by Jakob Petsovits <jpetso@gmx.at>
 */

// ------------------------------------------------------------
// Required customization
// ------------------------------------------------------------

// Base path of drupal directory (no trailing slash)
$xcvs['drupal_path'] = '/home/username/public_html';

// Location of the versioncontrol-bootstrap.php file.
$xcvs['bootstrap_path'] = $xcvs['drupal_path'] .'/sites/all/modules/versioncontrol/hooks/versioncontrol-bootstrap.php';

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


// ------------------------------------------------------------
// Internal code
// ------------------------------------------------------------
$stderr = fopen("php://stderr", "w");

// Bootstrap Drupal, providing module functions and database abstraction.
if (!file_exists($xcvs['bootstrap_path'])) {
  fwrite($stderr, "Error: failed to load Version Control API's bootstrap file.");
  exit(1);
}
include_once $xcvs['bootstrap_path'];
versioncontrol_bootstrap(DRUPAL_BOOTSTRAP_FULL);

// $xcvs has to be made global so the xcvs-taginfo.php
// script works properly.
global $xcvs_global;
$xcvs_global = $xcvs;
