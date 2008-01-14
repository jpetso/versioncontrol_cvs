<?php
// $Id$
/**
 * @file
 * CVS backend for Version Control API - Provides CVS commit information and
 * account management as a pluggable backend.
 *
 * This file contains the administrative user interface customizations
 * for accounts and repositories.
 *
 * Copyright 2006, 2007 Derek Wright ("dww" , http://drupal.org/user/46549)
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

define('VERSIONCONTROL_CVS_MIN_PASSWORD_LENGTH', 5);

/**
 * Implementation of hook_form_alter(): Add elements to various
 * administrative forms that the Version Control API provides.
 */
function versioncontrol_cvs_form_alter($form_id, &$form) {
  if ($form['#id'] == 'versioncontrol-repository-form' && $form['#vcs'] == 'cvs') {
    versioncontrol_cvs_repository_admin_form_alter($form_id, $form);
  }
  else if ($form['#id'] == 'versioncontrol-account-form' && $form['#vcs'] == 'cvs') {
    versioncontrol_cvs_account_form_alter($form_id, $form);
  }
}


/**
 * Add CVS specific elements to the add/edit repository form.
 */
function versioncontrol_cvs_repository_admin_form_alter($form_id, &$form) {
  $repository = $form['#repository'];

  $form['versioncontrol_cvs'] = array(
    '#type' => 'value',
    '#value' => TRUE,
  );
  $form['updated'] = array(
    '#type' => 'value',
    '#value' => isset($repository) ? $repository['cvs_specific']['updated'] : 0,
  );

  $form['repository_information']['root']['#description'] = t(
    'The CVSROOT of this repository. Examples: /path or :pserver:user:password@server:/path.'
  );
  $form['repository_information']['modules'] = array(
    '#type' => 'textfield',
    '#title' => t('Modules'),
    '#description' => t('Separate multiple CVS modules with spaces. If the "Automatic log retrieval" update method is enabled, only commits in these modules will be recorded. (Leave empty to record all commits, including those in CVSROOT.)'),
    '#default_value' => isset($repository) ? implode(' ', $repository['cvs_specific']['modules']) : '',
    '#weight' => 7,
    '#size' => 40,
    '#maxlength' => 255,
  );
  $form['repository_information']['update_method'] = array(
    '#type' => 'radios',
    '#title' => t('Update method'),
    '#description' => t('Automatic log retrieval requires cron.'),
    '#default_value' => isset($repository)
                        ? $repository['cvs_specific']['update_method']
                        : VERSIONCONTROL_CVS_UPDATE_CRON,
    '#weight' => 10,
    '#options' => array(
      VERSIONCONTROL_CVS_UPDATE_CRON => t('Automatic log retrieval.'),
      VERSIONCONTROL_CVS_UPDATE_XCVS => t('Use external script to insert data.'),
    ),
  );

  $form['cvs_export_information'] = array(
    '#type' => 'fieldset',
    '#title' => t('Account export information'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#weight' => 2,
  );
  $form['cvs_export_information']['run_as_user'] = array(
    '#type' => 'textfield',
    '#title' => t('Run as different system user'),
    '#description' => t('If this is empty, the exported account data will cause server-side CVS to be run with the system user corresponding to the authenticated CVS account name. If this field is not empty and you specify a different system username here, the exported account data will cause CVS to run as this user instead.'),
    '#default_value' => isset($repository)
                        ? $repository['cvs_specific']['run_as_user']
                        : 'drupal-cvs',
    '#weight' => 0,
    '#size' => 40,
    '#maxlength' => 255,
  );
}

/**
 * Implementation of hook_versioncontrol_extract_repository_data():
 * Extract CVS specific repository additions from the repository
 * editing/adding form's submitted values.
 */
function versioncontrol_cvs_versioncontrol_extract_repository_data($form_values) {
  if (!isset($form_values['versioncontrol_cvs'])) {
    return array();
  }
  $modules = trim($form_values['modules']);
  $modules = empty($modules) ? array() : explode(' ', $modules);

  return array(
    'cvs_specific' => array(
      'modules'       => $modules,
      'update_method' => $form_values['update_method'],
      'updated'       => $form_values['updated'],
      'run_as_user'   => $form_values['run_as_user'],
    ),
  );
}

/**
 * Implementation of hook_versioncontrol_alter_repository_list():
 * Add CVS specific columns into the list of CVS repositories.
 * By changing the @p $header and @p $rows_by_repo_id arguments,
 * the repository list can be customized accordingly.
 *
 * @param $vcs
 *   The unique string identifier for the version control system that
 *   the passed repository list covers.
 * @param $repositories
 *   An array of repositories of the given version control system.
 *   Array keys are the repository ids, and array values are the
 *   repository arrays like returned from versioncontrol_get_repository().
 * @param $header
 *   A list of columns that will be passed to theme('table').
 * @param $rows_by_repo_id
 *   An array of existing table rows, with repository ids as array keys.
 *   Each row already includes the generic column values, and for each row
 *   there is a repository with the same repository id given in the
 *   @p $repositories parameter.
 */
function versioncontrol_cvs_versioncontrol_alter_repository_list($vcs, $repositories, &$header, &$rows_by_repo_id) {
  if ($vcs != 'cvs') {
    return;
  }
  $header[] = t('Modules');
  $header[] = t('Update method');
  $header[] = t('Last updated');

  foreach ($rows_by_repo_id as $repo_id => $row) {
    $modules = array();
    foreach ($repositories[$repo_id]['cvs_specific']['modules'] as $module) {
      $modules[] = check_plain($module);
    }
    $rows_by_repo_id[$repo_id][] = theme('item_list', $modules);

    if ($repositories[$repo_id]['cvs_specific']['update_method'] == VERSIONCONTROL_CVS_UPDATE_XCVS) {
      $rows_by_repo_id[$repo_id][] = t('external script');
    }
    else if ($repositories[$repo_id]['cvs_specific']['update_method'] == VERSIONCONTROL_CVS_UPDATE_CRON) {
      $rows_by_repo_id[$repo_id][] = t('logs (!fetch)', array(
        '!fetch' => l(t('fetch now'), 'admin/project/versioncontrol-repositories/update/cvs/'. $repo_id)
      ));
    }
    $rows_by_repo_id[$repo_id][] = $repositories[$repo_id]['cvs_specific']['updated']
      ? format_date($repositories[$repo_id]['cvs_specific']['updated'], 'small')
      : t('never');
  }
}


/**
 * Add CVS specific elements to the edit/register user account form.
 */
function versioncontrol_cvs_account_form_alter($form_id, &$form) {
  $form['versioncontrol_cvs'] = array(
    '#type' => 'value',
    '#value' => TRUE,
  );

  if (empty($form['original_username']['#value'])) { // creating the account
    $description = t('Choose a password to access the CVS repository with.');
  }
  else { // editing the account
    $description = t('To change the current CVS password, enter the new password in both fields.');
  }
  $form['account']['account_password'] = array(
    '#type' => 'password_confirm',
    '#title' => t('CVS password'),
    '#description' => $description,
    '#weight' => 10,
  );
  $form['#validate']['versioncontrol_cvs_account_form_validate'] = array();
}

/**
 * Additional validation for the edit/register user account form.
 */
function versioncontrol_cvs_account_form_validate($form_id, $form_values) {
  if (!empty($form_values['original_username']) && empty($form_values['account_password'])) {
    return; // The (existing) user didn't change the password.
  }
  else if (strlen($form_values['account_password']) < VERSIONCONTROL_CVS_MIN_PASSWORD_LENGTH) {
    form_set_error('account_password', t('The CVS password you have chosen is too short (it must be at least !min characters long).', array('!min' => VERSIONCONTROL_CVS_MIN_PASSWORD_LENGTH)));
  }
}

/**
 * Implementation of hook_versioncontrol_extract_account_data():
 * Extract CVS specific user account additions (say: the password)
 * from the edit/register user account form's submitted values.
 */
function versioncontrol_cvs_versioncontrol_extract_account_data($form_values) {
  if (!isset($form_values['versioncontrol_cvs']) || empty($form_values['account_password'])) {
    return array();
  }
  return array(
    'cvs_specific' => array(
      'password' => crypt($form_values['account_password']),
    ),
  );
}
