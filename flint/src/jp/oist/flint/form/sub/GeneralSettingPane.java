/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 et: */
package jp.oist.flint.form.sub;

import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import jp.oist.flint.backend.ModelLoader;
import jp.oist.flint.quantity.TimeUnitConverter;
import jp.physiome.Ipc;
import jp.physiome.Ipc.ModelProbeResponse;
import java.io.File;
import java.io.IOException;
import java.util.Vector;
import java.util.concurrent.ExecutionException;

public class GeneralSettingPane extends javax.swing.JPanel 
    implements PropertyChangeListener {

    private final File mFile;
    private final String mFilePath;
    private final ModelLoader mLoader;
    private final ModelProbeResponse mModelProbeResponse;

    public GeneralSettingPane(File file, ModelLoader loader) 
            throws ExecutionException, InterruptedException, IOException {
        mFile = file;
        mFilePath = file.getCanonicalPath();
        mLoader = loader;

        initComponents();

        addPropertyChangeListener(this);
        mModelProbeResponse = loader.get();
    }

    public void setSelectedIntegrationMethodIndex (int i) {
        cmb_Integration_Method.setSelectedIndex(i);
    }

    public void setSelectedIntegrationMethod (Object v) {
        cmb_Integration_Method.setSelectedItem(v);
    }

    public int getSelectedIntegrationMethodIndex () {
        return cmb_Integration_Method.getSelectedIndex();
    }

    public void setInitializedLengthUnitItems (Vector v) {
        cmb_Length_Unit.removeAllItems();
        int length = v.size();
        for (int i=0; i<length; i++) 
            cmb_Length_Unit.addItem(v.get(i));
    }

    public void setSelectedLengthUnit (Object v) {
        cmb_Length_Unit.setSelectedItem(v);
    }

    public int getSelectedLengthUnitIndex () {
        return cmb_Length_Unit.getSelectedIndex();
    }

    public void setSelectedStepUnit(Object v) {
        cmb_Step_Unit.setSelectedItem(v);
    }

    public void setInitializedStepUnitItems (Vector v) {
        cmb_Step_Unit.removeAllItems();
        int length = v.size();
        for (int i=0; i<length; i++) 
            cmb_Step_Unit.addItem(v.get(i));
    }

    public int getSelectedStepUnitIndex () {
        return cmb_Step_Unit.getSelectedIndex();
    }

    public void setGranularity (Object v) {
        list_Granularity.setValue(v);
    }

    public String getSimulationLength () {
        return txt_Simulation_Length.getText();
    }

    public void setSimulationLegnth (String v) {
        txt_Simulation_Length.setText(v);
    }

    public String getSimulationStep () {
        return txt_Simulation_Step.getText();
    }

    public void setSimulationStep (String v) {
        txt_Simulation_Step.setText(v);
    }

    public Ipc.IntegrationMethod getIntegrationMethod() {
        return Ipc.IntegrationMethod.valueOf(this.cmb_Integration_Method.getSelectedIndex());
    }

    public String getLength() {
        return TimeUnitConverter.convert(txt_Simulation_Length.getText(),
                                         getSelectedLengthUnit(),
                                         mModelProbeResponse.getStepUnit());
    }

    public String getStep() {
        return TimeUnitConverter.convert(txt_Simulation_Step.getText(),
                                         getSelectedStepUnit(),
                                         mModelProbeResponse.getStepUnit());
    }

    public int getGranularity() {
        return Integer.parseInt(list_Granularity.getValue().toString());
    }

    private Ipc.TimeUnit getSelectedLengthUnit() {
        return mModelProbeResponse.getTimeUnit(cmb_Length_Unit.getSelectedIndex());
    }

    private Ipc.TimeUnit getSelectedStepUnit() {
        return mModelProbeResponse.getTimeUnit(cmb_Step_Unit.getSelectedIndex());
    }

    @Override
    public void propertyChange (PropertyChangeEvent evt) {
        String propertyName = evt.getPropertyName();
        if ("enabled".equals(propertyName)) {
            Boolean enabled = (Boolean)evt.getNewValue();
            cmb_Integration_Method.setEnabled(enabled);
            cmb_Length_Unit.setEnabled(enabled);
            cmb_Step_Unit.setEnabled(enabled);

            lbl_Data_Sampling.setEnabled(enabled);
            lbl_Integration_Method.setEnabled(enabled);
            lbl_Simulation_Length.setEnabled(enabled);
            lbl_Simulation_Step.setEnabled(enabled);
            lbl_data_per.setEnabled(enabled);
            lbl_steps.setEnabled(enabled);

            list_Granularity.setEnabled(enabled);

            txt_Simulation_Length.setEnabled(enabled);
            txt_Simulation_Step.setEnabled(enabled);
        }
    }

    /**
     * This method is called from within the constructor to initialize the form.
     * WARNING: Do NOT modify this code. The content of this method is always
     * regenerated by the Form Editor.
     */
    @SuppressWarnings("unchecked")
    // <editor-fold defaultstate="collapsed" desc="Generated Code">//GEN-BEGIN:initComponents
    private void initComponents() {

        pnl_Numerical_Integration_Method_Forms = new javax.swing.JPanel();
        pnl_Numerial_Integration_Method = new javax.swing.JPanel();
        lbl_Integration_Method = new javax.swing.JLabel();
        cmb_Integration_Method = new javax.swing.JComboBox();
        pnl_Simulation_Length = new javax.swing.JPanel();
        lbl_Simulation_Length = new javax.swing.JLabel();
        txt_Simulation_Length = new javax.swing.JTextField();
        spacer0 = new javax.swing.JPanel();
        cmb_Length_Unit = new javax.swing.JComboBox();
        pnl_Simulation_Time_Step = new javax.swing.JPanel();
        lbl_Simulation_Step = new javax.swing.JLabel();
        txt_Simulation_Step = new javax.swing.JTextField();
        spacer1 = new javax.swing.JPanel();
        cmb_Step_Unit = new javax.swing.JComboBox();
        pnl_Data_Output_Forms = new javax.swing.JPanel();
        pnl_Data_Sampling = new javax.swing.JPanel();
        lbl_Data_Sampling = new javax.swing.JLabel();
        lbl_data_per = new javax.swing.JLabel();
        list_Granularity = new javax.swing.JSpinner();
        lbl_steps = new javax.swing.JLabel();
        pnl_Enable_Labeled_Format = new javax.swing.JPanel();

        setAlignmentX(0.0F);
        setAlignmentY(0.0F);
        setMinimumSize(new java.awt.Dimension(480, 430));
        setPreferredSize(new java.awt.Dimension(480, 430));
        setLayout(new javax.swing.BoxLayout(this, javax.swing.BoxLayout.Y_AXIS));

        pnl_Numerical_Integration_Method_Forms.setBorder(javax.swing.BorderFactory.createTitledBorder(javax.swing.BorderFactory.createEtchedBorder(), "Numerical Integration"));
        pnl_Numerical_Integration_Method_Forms.setMaximumSize(new java.awt.Dimension(32768, 32768));
        pnl_Numerical_Integration_Method_Forms.setLayout(new javax.swing.BoxLayout(pnl_Numerical_Integration_Method_Forms, javax.swing.BoxLayout.Y_AXIS));

        pnl_Numerial_Integration_Method.setMaximumSize(new java.awt.Dimension(32767, 32));
        pnl_Numerial_Integration_Method.setMinimumSize(new java.awt.Dimension(0, 32));
        pnl_Numerial_Integration_Method.setPreferredSize(new java.awt.Dimension(713, 32));
        pnl_Numerial_Integration_Method.setRequestFocusEnabled(false);
        java.awt.FlowLayout flowLayout1 = new java.awt.FlowLayout(java.awt.FlowLayout.LEFT);
        flowLayout1.setAlignOnBaseline(true);
        pnl_Numerial_Integration_Method.setLayout(flowLayout1);

        lbl_Integration_Method.setHorizontalAlignment(javax.swing.SwingConstants.LEFT);
        lbl_Integration_Method.setText("Integration Method");
        lbl_Integration_Method.setMaximumSize(new java.awt.Dimension(180, 15));
        lbl_Integration_Method.setMinimumSize(new java.awt.Dimension(180, 15));
        lbl_Integration_Method.setPreferredSize(new java.awt.Dimension(180, 15));
        pnl_Numerial_Integration_Method.add(lbl_Integration_Method);

        cmb_Integration_Method.setModel(new javax.swing.DefaultComboBoxModel(new String[] { "Euler", "Runge-Kutta" }));
        cmb_Integration_Method.setMinimumSize(new java.awt.Dimension(150, 24));
        cmb_Integration_Method.setPreferredSize(new java.awt.Dimension(150, 24));
        pnl_Numerial_Integration_Method.add(cmb_Integration_Method);

        pnl_Numerical_Integration_Method_Forms.add(pnl_Numerial_Integration_Method);

        pnl_Simulation_Length.setMaximumSize(new java.awt.Dimension(32767, 32));
        pnl_Simulation_Length.setMinimumSize(new java.awt.Dimension(0, 32));
        pnl_Simulation_Length.setPreferredSize(new java.awt.Dimension(713, 32));
        java.awt.FlowLayout flowLayout6 = new java.awt.FlowLayout(java.awt.FlowLayout.LEFT);
        flowLayout6.setAlignOnBaseline(true);
        pnl_Simulation_Length.setLayout(flowLayout6);

        lbl_Simulation_Length.setText("Simulation Length");
        lbl_Simulation_Length.setMaximumSize(new java.awt.Dimension(180, 15));
        lbl_Simulation_Length.setMinimumSize(new java.awt.Dimension(180, 15));
        lbl_Simulation_Length.setPreferredSize(new java.awt.Dimension(180, 15));
        pnl_Simulation_Length.add(lbl_Simulation_Length);

        txt_Simulation_Length.setText("100");
        txt_Simulation_Length.setMinimumSize(new java.awt.Dimension(130, 19));
        txt_Simulation_Length.setPreferredSize(new java.awt.Dimension(130, 19));
        pnl_Simulation_Length.add(txt_Simulation_Length);

        spacer0.setMaximumSize(new java.awt.Dimension(8, 16));
        spacer0.setMinimumSize(new java.awt.Dimension(8, 16));
        spacer0.setPreferredSize(new java.awt.Dimension(8, 16));

        javax.swing.GroupLayout spacer0Layout = new javax.swing.GroupLayout(spacer0);
        spacer0.setLayout(spacer0Layout);
        spacer0Layout.setHorizontalGroup(
            spacer0Layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGap(0, 8, Short.MAX_VALUE)
        );
        spacer0Layout.setVerticalGroup(
            spacer0Layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGap(0, 16, Short.MAX_VALUE)
        );

        pnl_Simulation_Length.add(spacer0);

        cmb_Length_Unit.setModel(new javax.swing.DefaultComboBoxModel(new String[] { "milli_second", "second" }));
        cmb_Length_Unit.setMinimumSize(new java.awt.Dimension(120, 24));
        cmb_Length_Unit.setPreferredSize(new java.awt.Dimension(180, 24));
        pnl_Simulation_Length.add(cmb_Length_Unit);

        pnl_Numerical_Integration_Method_Forms.add(pnl_Simulation_Length);

        pnl_Simulation_Time_Step.setMaximumSize(new java.awt.Dimension(32767, 32));
        pnl_Simulation_Time_Step.setMinimumSize(new java.awt.Dimension(100, 32));
        pnl_Simulation_Time_Step.setPreferredSize(new java.awt.Dimension(713, 32));
        java.awt.FlowLayout flowLayout7 = new java.awt.FlowLayout(java.awt.FlowLayout.LEFT);
        flowLayout7.setAlignOnBaseline(true);
        pnl_Simulation_Time_Step.setLayout(flowLayout7);

        lbl_Simulation_Step.setText("Simulation Time Step");
        lbl_Simulation_Step.setMaximumSize(new java.awt.Dimension(180, 15));
        lbl_Simulation_Step.setMinimumSize(new java.awt.Dimension(180, 15));
        lbl_Simulation_Step.setPreferredSize(new java.awt.Dimension(180, 15));
        pnl_Simulation_Time_Step.add(lbl_Simulation_Step);

        txt_Simulation_Step.setText("0.01");
        txt_Simulation_Step.setMinimumSize(new java.awt.Dimension(130, 19));
        txt_Simulation_Step.setPreferredSize(new java.awt.Dimension(130, 19));
        txt_Simulation_Step.addActionListener(new java.awt.event.ActionListener() {
            public void actionPerformed(java.awt.event.ActionEvent evt) {
                txt_Simulation_StepActionPerformed(evt);
            }
        });
        pnl_Simulation_Time_Step.add(txt_Simulation_Step);

        spacer1.setMaximumSize(new java.awt.Dimension(8, 16));
        spacer1.setMinimumSize(new java.awt.Dimension(8, 16));

        javax.swing.GroupLayout spacer1Layout = new javax.swing.GroupLayout(spacer1);
        spacer1.setLayout(spacer1Layout);
        spacer1Layout.setHorizontalGroup(
            spacer1Layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGap(0, 8, Short.MAX_VALUE)
        );
        spacer1Layout.setVerticalGroup(
            spacer1Layout.createParallelGroup(javax.swing.GroupLayout.Alignment.LEADING)
            .addGap(0, 16, Short.MAX_VALUE)
        );

        pnl_Simulation_Time_Step.add(spacer1);

        cmb_Step_Unit.setModel(new javax.swing.DefaultComboBoxModel(new String[] { "milli_second", "second" }));
        cmb_Step_Unit.setMinimumSize(new java.awt.Dimension(120, 24));
        cmb_Step_Unit.setPreferredSize(new java.awt.Dimension(180, 24));
        pnl_Simulation_Time_Step.add(cmb_Step_Unit);

        pnl_Numerical_Integration_Method_Forms.add(pnl_Simulation_Time_Step);

        add(pnl_Numerical_Integration_Method_Forms);

        pnl_Data_Output_Forms.setBorder(javax.swing.BorderFactory.createTitledBorder(javax.swing.BorderFactory.createEtchedBorder(), "Output"));
        pnl_Data_Output_Forms.setMaximumSize(new java.awt.Dimension(32768, 32768));
        pnl_Data_Output_Forms.setLayout(new javax.swing.BoxLayout(pnl_Data_Output_Forms, javax.swing.BoxLayout.Y_AXIS));

        pnl_Data_Sampling.setAlignmentY(0.0F);
        pnl_Data_Sampling.setMaximumSize(new java.awt.Dimension(32767, 26));
        pnl_Data_Sampling.setMinimumSize(new java.awt.Dimension(318, 26));
        java.awt.FlowLayout flowLayout3 = new java.awt.FlowLayout(java.awt.FlowLayout.LEFT, 4, 5);
        flowLayout3.setAlignOnBaseline(true);
        pnl_Data_Sampling.setLayout(flowLayout3);

        lbl_Data_Sampling.setLabelFor(list_Granularity);
        lbl_Data_Sampling.setText("Data Sampling");
        lbl_Data_Sampling.setMaximumSize(new java.awt.Dimension(180, 15));
        lbl_Data_Sampling.setMinimumSize(new java.awt.Dimension(180, 15));
        lbl_Data_Sampling.setPreferredSize(new java.awt.Dimension(180, 15));
        pnl_Data_Sampling.add(lbl_Data_Sampling);

        lbl_data_per.setText("1 data per ");
        pnl_Data_Sampling.add(lbl_data_per);

        list_Granularity.setModel(new javax.swing.SpinnerNumberModel(Integer.valueOf(1), Integer.valueOf(1), null, Integer.valueOf(1)));
        list_Granularity.setAutoscrolls(true);
        list_Granularity.setMaximumSize(new java.awt.Dimension(32767, 20));
        list_Granularity.setMinimumSize(new java.awt.Dimension(35, 20));
        list_Granularity.setPreferredSize(new java.awt.Dimension(60, 20));
        list_Granularity.setValue(1);
        pnl_Data_Sampling.add(list_Granularity);

        lbl_steps.setText("step(s)");
        pnl_Data_Sampling.add(lbl_steps);

        pnl_Data_Output_Forms.add(pnl_Data_Sampling);

        pnl_Enable_Labeled_Format.setAlignmentY(0.0F);
        pnl_Enable_Labeled_Format.setAutoscrolls(true);
        pnl_Enable_Labeled_Format.setMaximumSize(new java.awt.Dimension(32767, 26));
        pnl_Enable_Labeled_Format.setMinimumSize(new java.awt.Dimension(248, 26));
        java.awt.FlowLayout flowLayout4 = new java.awt.FlowLayout(java.awt.FlowLayout.LEFT, 0, 5);
        flowLayout4.setAlignOnBaseline(true);
        pnl_Enable_Labeled_Format.setLayout(flowLayout4);
        pnl_Data_Output_Forms.add(pnl_Enable_Labeled_Format);

        add(pnl_Data_Output_Forms);
    }// </editor-fold>//GEN-END:initComponents

    private void txt_Simulation_StepActionPerformed(java.awt.event.ActionEvent evt) {//GEN-FIRST:event_txt_Simulation_StepActionPerformed
        // TODO add your handling code here:
    }//GEN-LAST:event_txt_Simulation_StepActionPerformed


    // Variables declaration - do not modify//GEN-BEGIN:variables
    private javax.swing.JComboBox cmb_Integration_Method;
    private javax.swing.JComboBox cmb_Length_Unit;
    private javax.swing.JComboBox cmb_Step_Unit;
    private javax.swing.JLabel lbl_Data_Sampling;
    private javax.swing.JLabel lbl_Integration_Method;
    private javax.swing.JLabel lbl_Simulation_Length;
    private javax.swing.JLabel lbl_Simulation_Step;
    private javax.swing.JLabel lbl_data_per;
    private javax.swing.JLabel lbl_steps;
    private javax.swing.JSpinner list_Granularity;
    private javax.swing.JPanel pnl_Data_Output_Forms;
    private javax.swing.JPanel pnl_Data_Sampling;
    private javax.swing.JPanel pnl_Enable_Labeled_Format;
    private javax.swing.JPanel pnl_Numerial_Integration_Method;
    private javax.swing.JPanel pnl_Numerical_Integration_Method_Forms;
    private javax.swing.JPanel pnl_Simulation_Length;
    private javax.swing.JPanel pnl_Simulation_Time_Step;
    private javax.swing.JPanel spacer0;
    private javax.swing.JPanel spacer1;
    private javax.swing.JTextField txt_Simulation_Length;
    private javax.swing.JTextField txt_Simulation_Step;
    // End of variables declaration//GEN-END:variables
}
