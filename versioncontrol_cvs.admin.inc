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
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

/**
 * Return a user visible text that contains one or more examples for
 * how the repository root should look like. This string will show up in the
 * Add/Edit FakeVCS repository of the repository administration pages.
 */
function versioncontrol_cvs_get_repository_root_example() {
  return t('Examples: /path or :pserver:user:password@server:/path.');
}

/**
 * Implementation of hook_form_alter():
 * Add CVS specific elements to the repository adding/editing form.
 */
function versioncontrol_cvs_form_alter($form_id, &$form) {
  if ($form['#id'] == 'repository-form' && $form['#vcs'] == 'cvs') {
    $repository = $form['#repository'];

    $form['repository_information']['updated'] = array(
      '#type' => 'value',
      '#value' => isset($repository) ? $repository['cvs_specific']['updated'] : 0,
    );
    $form['repository_information']['modules'] = array(
      '#type' => 'textfield',
      '#title' => t('Modules'),
      '#description' => t('Separate multiple CVS modules with spaces.'),
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
                          : VERSIONCONTROL_CVS_UPDATE_XCVS,
      '#weight' => 10,
      '#options' => array(
        // log retrieval is not yet ported from cvs.module
        // VERSIONCONTROL_CVS_UPDATE_CRON => t('Automatic log retrieval.'),
        VERSIONCONTROL_CVS_UPDATE_XCVS => t('Use external script to insert data.'),
      ),
    );
  }
}

/**
 * Implementation of [versioncontrol_backend]_extract_repository_vcs_specific():
 * Extract CVS specific repository additions from the repository
 * editing/adding form's submitted values.
 */
function versioncontrol_cvs_extract_repository_vcs_specific($form_values) {
  $modules = trim($form_values['modules']);
  $modules = empty($modules) ? array() : explode(' ', $modules);
  return array(
    'modules'       => $modules,
    'update_method' => $form_values['update_method'],
    'updated'       => $form_values['updated'],
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
 *   An array of existing rows, with repository ids as array keys.
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
    $modules = $repositories[$repo_id]['cvs_specific']['modules'];
    $rows_by_repo_id[$repo_id][] = theme('item_list', $modules);

    if ($repositories[$repo_id]['cvs_specific']['update_method'] == VERSIONCONTROL_CVS_UPDATE_XCVS) {
      $rows_by_repo_id[$repo_id][] = t('external script');
      $rows_by_repo_id[$repo_id][] = t('n/a');
    }
    else if ($repositories[$repo_id]['cvs_specific']['update_method'] == VERSIONCONTROL_CVS_UPDATE_CRON) {
      $rows_by_repo_id[$repo_id][] = t('logs (!fetch)', array(
        '!fetch' => l(t('fetch now'), 'admin/project/versioncontrol-repositories/update/'. $repo_id)
      ));
      $rows_by_repo_id[$repo_id][] = $repositories[$repo_id]['cvs_specific']['updated']
                                     ? format_date($repositories[$repo_id]['cvs_specific']['updated'])
                                     : t('never');
    }
  }
}
