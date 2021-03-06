<?php

/**
 * Get GO terms for a given feature as stored by this module
 *
 * @param $feature_id
 *   The feature_id of the feature
 */
function tripal_get_feature_go_results($feature_id) {

   $sql = "
       SELECT DISTINCT FCVT.feature_id,DBX.accession,CVT.name as goterm,
          CVT.cvterm_id as go_id, CV.name as cvname, CVT.definition
       FROM {Feature_Cvterm} FCVT
          INNER JOIN {Cvterm} CVT           ON CVT.cvterm_ID = FCVT.cvterm_ID
          INNER JOIN {CV}                   ON CV.cv_id = CVT.cv_id
          INNER JOIN {dbxref} DBX           ON DBX.dbxref_id = CVT.dbxref_id
        WHERE
          (CV.name = 'biological_process' OR
           CV.name = 'cellular_component' OR
           CV.name = 'molecular_function') AND
          FCVT.feature_id = :feature_id
        ORDER BY CV.name, CVT.name
   ";

   $results = chado_query($sql, array(':feature_id' => $feature_id));
   $i=0;
   $terms = array();
   while($term = $results->fetchObject()){

      $terms[$i++] = $term;
   }
   return $terms;

  // Get the go results stored as XML from the analysisfeatureprop table
  // the type for the property is named 'analysis_blast_output_iteration_hits'
  // and is found in the 'tripal' controlled vocabulary.  This CV term was
  // added by this module.
  $select = array(
    'analysisfeature_id' => array(
      'feature_id' => $feature_id,
    ),
    'type_id' => array(
      'name' => 'analysis_blast_output_iteration_hits',
      'cv_id' => array(
        'name' => 'tripal'
      ),
    ),
  );
  $blast_results = chado_select_record('analysisfeatureprop', array('*'), $select);
  if (!$blast_results) {
    return;
  }

  // get the HTML content for viewing each of the XML file
  $blast_obj_array = array();
  $blast_obj_counter = 0;
  foreach ($blast_results as $index => $analysisfeatureprop) {
    // get the blast XML for this feature
    $blast_xml = $analysisfeatureprop->value;

    // get the analysis record
    $analysisfeature_arr = chado_select_record('analysisfeature', array('analysis_id'),
        array('analysisfeature_id' => $analysisfeatureprop->analysisfeature_id));
    $analysis_arr = chado_select_record('analysis', array('*'),
        array('analysis_id' => $analysisfeature_arr[0]->analysis_id));
    $analysis = $analysis_arr[0];
    $analysis_id = $analysis->analysis_id;

    // the old style was to store all parameters in a single CV term in the analysisprop
    // table. However now each property has it's own CV term in that table. But,
    // we still need to support the old method for backwards compatibility.
    // so,  first get the old style variable and see if it has values. In
    // particular we need the database setting
    $blast_settings  = chado_get_property(array('table'=> 'analysis', 'id' => $analysis_id), array('type_name' => 'analysis_blast_settings', 'cv_name' => 'tripal'));
    if ($blast_settings) {
      $blastsettings = explode("|",   $blast_settings->value);
      // if we don't have the proper number of fields in the value column then
      // skip this entry
      if (count($blastsettings) != 3) {
        continue;
      }
      $adb_id = $blastsettings[0];
    }
    // if we're not using the old style then try the new method to get the
    // database id
    else {
      $blastdb = chado_get_property(array('table'=> 'analysis', 'id' => $analysis_id), array('type_name' => 'analysis_blast_blastdb', 'cv_name' => 'tripal'));
      $adb_id = $blastdb->value;
    }
    // if the callee specified a database to show then we want to check that
    // with the database id of the analysis we're looking at.  If they
    // don't match then skip this blast.  If a database id was not specified
    // then continue
    if ($db_id and $adb_id != $db_id) {
      continue;
    }

    // get the database
    if ($adb_id) {
      $db_arr = chado_select_record('db', array('*'), array('db_id' => $adb_id));
      $db = $db_arr[0];
    }

    // parse the XML and add it to the array of blast results to be returned
    $blast_obj = tripal_analysis_blast_get_result_object($blast_xml, $db, $feature_id, $analysis);
    $blast_obj->analysis = $analysis;
    $blast_obj_array [$blast_obj_counter] = $blast_obj;
    $blast_obj_counter ++;
  }

  return $blast_obj_array;
}
