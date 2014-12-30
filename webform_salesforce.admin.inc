<?php
/**
 * @file
 * This file contains the admin and GUI callbacks for the module that are not needed in day-to-day operations
 */

function webform_salesforce_configure_page($node) {
  return drupal_get_form('webform_salesforce_configure_form', $node);
}

/*
 * Implements callback for form construction
 */
function webform_salesforce_configure_form($form, $form_state, $node) {
  $form = array();
  $form_cfg = webform_salesforce_get_map($node);
  $map = (!empty($form_cfg['map'])) ? $form_cfg['map'] : array();
  $def_object = (!empty($form_cfg['object'])) ? $form_cfg['object'] : '';

  // Get all the allowed SF objects
  $sf_options = webform_salesforce_describe_objects();

  if (!empty($sf_options)) {
    // Get all available SF objects
    $sf_list = array();
    foreach ($sf_options as $key => $data) {
      $sf_list[$key] = $data['value'];
    }

    $form['sf_setup'] = array(
      '#type' => 'fieldset',
      '#title' => t('Salesforce Settings'),
      '#tree' => TRUE,
    );
    $form['sf_setup']['enabled'] = array(
      '#type' => 'checkbox',
      '#default_value' => !empty($def_object) ? 1 : 0,
      '#title' => t('Enabled'),
      '#description' => t('Field mapping and object selection are only stored if Salesforce is enabled for this Webform.'),
    );
    $form['sf_setup']['sf_object'] = array(
      '#type' => 'radios',
      '#title' => t('Salesforce Object'),
      '#description' => t('Please select which object will be created in Salesform via this Webform.'),
      '#options' => $sf_list,
      '#default_value' => $def_object,
      '#required' => FALSE,
    );

    // Get all available Webform fields and build option list for mapping
    if (!empty($node->webform['components'])) {
      $wf_components = array('' => '- Select -');
      foreach ($node->webform['components'] as $field) {
        $name = $field['name'];
        if ($field['mandatory']) {
          $name .= ' *';
        }
        $wf_components[$field['form_key']] = $name;
      }
    }
    else {
      drupal_set_message(t('Please add some webform form components!'), 'notice');
    }

    // Do not show mapping unless there are some webform components
    if (isset($wf_components) && !empty($def_object)) {

      // Get field definitions for the selected object
      $sf_object_fields = webform_salesforce_get_available_fields($def_object);

      $form['sf_fields_mappings_wrapper'] = array(
        '#type' => 'fieldset',
        '#title' => t('Field Mapping'),
        '#tree' => TRUE,
      );
      $form['sf_fields_mappings_wrapper']['info'] = array(
        '#markup' => t('<p>Select the Webform component that should be mapped to the Salesforce field.<br />All required fields are designated with a <strong>*</strong> next to their name.</p>'),
      );
      $form['sf_fields_mappings_wrapper']['salesforce_field_mappings'] = array(
        '#theme' => 'webform_salesforce_mapping_form_table',
        '#tree' => TRUE,
        '#header' => array(
          'salesforce_field' => t('Salesforce field'),
          'webform_component' => t('Webform component'),
        ),
      );

      foreach ($sf_object_fields as $key => $attr) {
        $row_id = 'edit-salesforce-field-mappings-' . $key;
        $form['salesforce_field_mappings_wrapper']['salesforce_field_mappings'][$key] = array(
          '#type' => 'container',
          '#attributes' => array(
            'id' => array($row_id),
          ),
        );
        $row = &$form['sf_fields_mappings_wrapper']['salesforce_field_mappings'][$key];

        // Rendering Salesforce field label
        $label = $attr['required'] ? check_plain($attr['label']) . ' <span class="form-required">(' . t('Required') . ')</span>' : check_plain($attr['label']);
        $row['salesforce_field'] = array(
          '#id' => 'edit-salesforce-field-' . $key,
          '#markup' => $label,
        );

        // Rendering Webform component select field.
        $defval = (!empty($map[$attr['fieldid']]) ? $map[$attr['fieldid']] : '');
        $row['webform_component'] = array(
          '#type' => 'select',
          '#title' => check_plain($attr['label']),
          '#options' => $wf_components,
          '#default_value' => $defval,
          '#parents' => array('sf_fields', $key),
        );

        // Internal field with Salesforce required status.
        $row['sf_required'] = array(
          '#type' => 'value',
          '#value' => $attr['required'] ? 1 : 0,
        );
      }
    }

    // Set node info as it doesn't get passed to the submit handler
    $form['nid'] = array(
      '#type' => 'hidden',
      '#value' => $node->nid,
    );
    $form['vid'] = array(
      '#type' => 'hidden',
      '#value' => (!empty($node->vid)) ? $node->vid : 0,
    );
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Save'),
    );
    $form['clear'] = array(
      '#type' => 'submit',
      '#value' => 'Reset Form',
      '#validate' => array('webform_salesforce_configure_form_clear'),
    );
  }
  else {
    $form['msg'] = array(
      '#type' => 'markup',
      '#value' => '<p>' . t('No fields are defined for this object, or else there was an error retrieving the fields.  Please !check_log_link for errors.', array('!check_log_link' => l(t('check the log'), 'admin/reports/dblog'))) . '</p>',
    );
  }
  return $form;
}

/*
 * Validate function for our reset button
 */
function webform_salesforce_configure_form_clear($form, &$form_state) {
  unset($form_state['values']);
  unset($form_state['storage']);

  $form_state['rebuild'] = TRUE;
}

/*
 * Implement callback for validation of form 'webform_salesforce_configure_form'
 */
function webform_salesforce_configure_form_validate($form, &$form_state) {
  $values = $form_state['values'];
  if ($values['sf_setup']['enabled'] == 1 && isset($values['sf_fields'])) {
    foreach ($values['sf_fields'] as $id => $val) {
      if (isset($form['sf_fields_mappings_wrapper']['salesforce_field_mappings'])) {
        $sf_mapping = $form['sf_fields_mappings_wrapper']['salesforce_field_mappings'];
        if ($sf_mapping[$id]['sf_required']['#value'] && empty($val)) {
          form_set_error("sf_fields][$id", t('No source Webform component selected for required Salesforce field %field.  Please select a Webform component from the dropdown.', array('%field' => $sf_mapping[$id]['webform_component']['#title'])));
        }
      }
    }
  }
}

/*
 * Implement callback for submit handling of form 'webform_salesforce_configure_form'
 */
function webform_salesforce_configure_form_submit($form, &$form_state) {
 // Only store mapping if SF is enabled
  $values = $form_state['values'];
  $node = node_load($form_state['values']['nid'], $form_state['values']['vid']);
  if ($values['sf_setup']['enabled'] == 1) {
    $mapping['object'] = $values['sf_setup']['sf_object'];
    if (!empty($values['sf_fields'])) {
      foreach ($values['sf_fields'] as $key => $val) {
        if (!empty($val)) {
          $mapping['map'][$key] = $val;
        }
      }
    }

    _webform_salesforce_set_map($node, $mapping);
  }
  else {
    _webform_salesforce_delete_map($node);
  }
  drupal_set_message(t('Webform Salesforce settings successfully saved.'), 'status');
}

/**
 * Themes the field / webform component mapping table.
 */
function theme_webform_salesforce_mapping_form_table($variables) {
  $elements = $variables['elements'];

  // Build the rows array.
  $columns = isset($elements['#columns']) ? $elements['#columns'] : (isset($elements['#header']) ? array_keys($elements['#header']) : array());
  $rows = array();
  foreach (element_children($elements) as $child_key) {
    $child = & $elements[$child_key];
    $data = array();
    $row_columns = empty($columns) ? element_children($child) : $columns;
    foreach ($row_columns as $column) {
      if (isset($child[$column])) {
        // Removing label from select fields
        if (isset($child[$column]['#type'])) {
          if ($child[$column]['#type'] == 'select') {
            $child[$column]['#title'] = '';
          }
        }
        $cell = array('data' => drupal_render($child[$column]));
        if (isset($child[$column]['#attributes'])) {
          foreach ($child[$column]['#attributes'] as $key => $value) {
            $cell[$key] = ($key == 'id') ? (is_array($value) ? array($value[0] . '-cell') : $value . '-cell') : $value;
          }
        }
        $data[] = $cell;
      }
    }
    $row = array('data' => $data);
    if (isset($child['#attributes'])) {
      foreach ($child['#attributes'] as $key => $value) {
        $row[$key] = $value;
      }
    }
    $rows[] = $row;
  }

  $config = array(
    'rows' => $rows,
  );
  if (isset($elements['#header'])) {
    $config['header'] = $elements['#header'];
  }
  if (isset($elements['#attributes']) && is_array($elements['#attributes'])) {
    $config['attributes'] = $elements['#attributes'];
  }

  return theme('table', $config);
}