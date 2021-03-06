<?php
/**
 * @file
 * Adds support for Backdrop Profile module (formerly Drupal Profile 2).
 * Supplemental include loaded via usermerge_load_includes().
 */

/**
 * Implements hook_usermerge_actions_supported().
 */
function profile_usermerge_actions_supported() {
  return array(
    'profile' => t('Merge Profile profiles.'),
  );
}


/**
 * Implements hook_usermerge_account_properties().
 */
function profile_usermerge_account_properties($user_to_delete, $user_to_keep, $action) {
  // Set the account properties.
  $account_properties = array();

  // Retrieve all bundles for the profile.
  $bundles = field_info_bundles('profile');

  // Set the profile fields as properties.
  foreach ($bundles as $bundle => $bundle_info) {
    // Set the bundle information.
    $account_properties[$bundle] = array(
      'title' => t('Profile - @label', array('@label' => $bundle_info['label'])),
      'type' => $bundle,
      'items' => array(),
    );

    // Add all fields from that bundle.
    foreach (field_info_instances('profile', $bundle) as $name => $info) {
      $field_info = field_info_field($name);
      $item = array(
        'label' => str_replace(':', '', $info['label']),
        'cardinality' => $field_info['cardinality'],
      );

      // Set the item.
      $account_properties[$bundle]['items'][$name] = $item;
    }
  }

  return array('profile' => $account_properties);
}


/**
 * Implements hook_usermerge_build_review_form_elements().
 */
function profile_usermerge_build_review_form_elements($review, $account_properties, $user_to_delete, $user_to_keep) {
  $review['profile'] = array();

  // Iterate through each bundle.
  foreach ($account_properties['profile'] as $bundle => $properties) {
    // Generate form based on items.
    $review['profile'][$bundle] = _profile_usermerge_build_review_form_elements_by_bundle($properties, $user_to_delete, $user_to_keep);
  }

  return $review;
}


/**
 * Pass in review elements per bundle.
 *
 * @param array $properties
 *   Entity properties.
 * @param object $user_to_delete
 *   User entity to delete.
 * @param object $user_to_keep
 *   User entity to keep.
 *
 * @return array
 *   The form array to render.
 */
function _profile_usermerge_build_review_form_elements_by_bundle($properties, $user_to_delete, $user_to_keep) {
  $review = array();

  // Set up profile tree..
  $review = array(
    '#tree' => TRUE,
    '#theme' => 'usermerge_data_review_form_table',
    '#title' => t('@title (!module)',
      array('@title' => $properties['title'], '!module' => t('Profile'))),
    '#weight' => -10,
  );

  // Load the profile for the user to delete and keep.
  $profile_delete = profile_load_by_user($user_to_delete, $properties['type']);
  $profile_keep = profile_load_by_user($user_to_keep, $properties['type']);

  // Do nothing if both of the profiles don't exist.
  if (empty($profile_delete) && empty($profile_keep)) {
    $review['no_info']['property_name'] = array(
      '#type' => 'markup',
      '#markup' => t('Profile'),
    );

    $review['no_info']['options']['user_to_delete'] = array(
      '#type' => 'markup',
      '#markup' => t('No Profiles Found - profile information will not be imported'),
    );

    return $review;
  }

  foreach ($properties['items'] as $name => $info) {
    $review[$name]['property_name'] = array(
      '#type' => 'markup',
      '#markup' => $info['label'],
    );

    // Init the values we will be displaying.
    $values = array(
      'delete' => _profile_usermerge_process_field($profile_delete, $name),
      'keep' => _profile_usermerge_process_field($profile_keep, $name),
    );

    $review[$name]['options'] = array(
      '#type' => 'radios',
      '#options' => array(
        'user_to_delete' => implode('<br />', $values['delete']),
        'user_to_keep' => implode('<br />', $values['keep']),
        'merge' => $info['cardinality'] != 1 && $values['delete'] != $values['keep'] ? 'merge' : 'no_merge',
      ),
      '#default_value' => 'user_to_keep',
    );
    $review[$name]['cardinality'] = array(
      '#type' => 'hidden',
      '#value' => $info['cardinality'],
    );

    if (isset($info['criterion'])) {
      unset($review[$name]['options']['#options']);
      $review[$name]['options']['#options']['merge'] = 'merge';
      $review[$name]['options']['#default_value'] = 'merge';
    }
  }

  return $review;
}


/**
 * Process all fields.
 *
 * @param object $profile
 *   The profile entity.
 * @param string $name
 *   The name of the field.
 *
 * @return array
 *   Array of field values.
 */
function _profile_usermerge_process_field($profile, $name) {
  // Default to empty text.
  $ret = array();

  if ($profile) {
    // Retrieve the field values.
    $values = field_get_items('profile', $profile, $name);

    // Check that field_get_items returns an array.
    if (is_array($values)) {
      foreach ($values as $value) {
        $view = field_view_value('profile', $profile, $name, $value);

        // Markup.
        if (isset($view['#markup'])) {
          // Take up to 50 characters from the markup.
          $ret[] = backdrop_substr(strip_tags($view['#markup']), 0, 50);
        }
        // Title.
        elseif (isset($view['#title'])) {
          $ret[] = backdrop_substr(strip_tags($view['#title']), 0, 50);
        }
        // Otherwise render it.
        else {
          $ret[] = backdrop_render($view);
        }
      }
    }
  }
  return $ret;
}


/**
 * Implements hook_usermerge_merge_accounts().
 */
function profile_usermerge_merge_accounts($user_to_delete, $user_to_keep, $review) {
  if (empty($review['profile'])) {
    return;
  }
  // Iterate through each bundle.
  foreach ($review['profile'] as $bundle => $options) {
    // User merge on each bundle.
    _profile_usermerge_merge_accounts_by_bundle($bundle, $user_to_delete, $user_to_keep, $options);
  }
}


/**
 * Merge accounts by profile bundle name.
 *
 * @param string $bundle
 *   Profile2 bundle name.
 * @param object $user_to_delete
 *   User entity to delete.
 * @param object $user_to_keep
 *   User entity to keep.
 *
 * @return array
 *   The form array to render.
 */
function _profile_usermerge_merge_accounts_by_bundle($bundle, $user_to_delete, $user_to_keep, $review) {
  // Load the profile for the user to delete and keep.
  $profile_delete = profile_load_by_user($user_to_delete, $bundle);
  $profile_keep = profile_load_by_user($user_to_keep, $bundle);

  // Do nothing if both of the profiles do not exist; otherwise create any missing profiles.
  if (empty($profile_delete) && empty($profile_keep)) {
    return;
  }
  if (empty($profile_delete)) {
    $profile_delete = profile_create(array('type' => $bundle, 'uid' => $user_to_delete->uid));
  }
  if (empty($profile_keep)) {
    $profile_keep = profile_create(array('type' => $bundle, 'uid' => $user_to_keep->uid));
  }

  // ===============
  // Merge fields based on review.
  // ===============
  foreach ($review as $name => $options) {
    $option = $options['options'];

    // Switch on the name.
    switch ($option) {
      // Get value from merge (nust be an array).
      case 'merge':
        $value = array();
        $field_delete = isset($profile_delete->$name) ? $profile_delete->$name : array();
        $field_keep = isset($profile_keep->$name) ? $profile_keep->$name : array();

        // Retrieve the languages.
        $languages = array_unique(array_merge(array_keys($field_delete), array_keys($field_keep)));

        // Set language keys.
        foreach ($languages as $lang) {
          // Init to array.
          $value[$lang] = array();

          // Add user to keep items first.
          if (!empty($field_keep[$lang])) {
            $value[$lang] = $field_keep[$lang];
          }

          // Add user to delete items, remove duplicates.
          if (!empty($field_delete[$lang])) {
            $value[$lang] = array_merge($value[$lang], $field_delete[$lang]);

            // Remove duplicates.
            $serialized = array_map('serialize', $value[$lang]);
            $serialized = array_values(array_unique($serialized));
            $value[$lang] = array_map('unserialize', $serialized);
          }

          // Enforce maximum number of values.
          if ($options['cardinality'] > 1) {
            $value[$lang] = array_slice($value[$lang], 0, $options['cardinality']);
          }
        }
        break;

        // Get value from delete.
      case 'user_to_delete':
        $value = isset($profile_delete->$name) ? $profile_delete->$name : NULL;
        break;

        // User to keep is the default case.
      case 'user_to_keep':
      default:
        $value = isset($profile_keep->$name) ? $profile_keep->$name : NULL;
        break;
    }

    // Set the value.
    $profile_keep->$name = $value;
  }

  // Save the updated profile.
  profile_save($profile_keep);

  // Delete the profile for the deleted user.
  profile_delete($profile_delete);
}


/**
 * Implements hook_usermerge_query_authored_entities_alter().
 */
function profile_usermerge_query_authored_entities_alter(&$entities) {
  // Prevent profile entities from being automatically merged as authored
  // entities because Profile supplies its own merging.
  unset($entities['profile']);
}
