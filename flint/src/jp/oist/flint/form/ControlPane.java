/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 et: */

package jp.oist.flint.form;

import jp.oist.flint.desktop.ILoadingListener;
import jp.oist.flint.desktop.ISimulationListener;
import jp.oist.flint.executor.PhspSimulator;
import java.awt.BorderLayout;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.FocusEvent;
import java.awt.event.FocusListener;
import javax.swing.BorderFactory;
import javax.swing.JPanel;
import javax.swing.JButton;

public class ControlPane extends PeripheralPane 
    implements FocusListener, ILoadingListener, ISimulationListener {

    public final static String RUN = "controlpanel.run";

    public final static String TITLE  = "Control Panel";

    private final static ControlPane mInstance = new ControlPane();

    public static ControlPane getInstance () {
        return mInstance;
    }

    private JButton mBtnSimulationRun;

    private ControlPane () {
        super(TITLE);

        initComponents();
        addFocusListener(this);
    }

    private void initComponents () {
        setContentPane(createContentPane());
    }

    protected Container createContentPane () {
        JPanel contentPane = new JPanel(new BorderLayout());
        contentPane.setBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10));

        setPreferredSize(new Dimension(Short.MAX_VALUE, 75));
        mBtnSimulationRun = new JButton("Run");
        mBtnSimulationRun.setName(RUN);
        mBtnSimulationRun.setEnabled(false);
        mBtnSimulationRun.addActionListener(new ActionListener () {
            @Override
            public void actionPerformed(ActionEvent e) {
                simulationRunPerformed(e);
            }
        });
        contentPane.add(mBtnSimulationRun, BorderLayout.CENTER);

        return contentPane;
    }

    protected void simulationRunPerformed (ActionEvent e) {
        MenuBar menuBar = MenuBar.getInstance();
        IMenuDelegator delegator = menuBar.getDelegator();
        if (delegator != null)
            delegator.simulationRunPerformed(e);
    }

    @Override
    public void focusGained(FocusEvent e) {
        mBtnSimulationRun.requestFocus();
    }

    @Override
    public void focusLost(FocusEvent e) {
    }

    public void setSimulationRunEnabled(boolean enabled) {
        mBtnSimulationRun.setEnabled(enabled);
        MenuBar.getInstance().setMenuItemEnabled(MenuBar.RUN, enabled);
        MenuBar.getInstance().setMenuItemEnabled(MenuBar.SEND_TO_FLINT_K3, enabled);
    }

    /* ILoadingListener */

    @Override
    public void loadingStarted() {
        setSimulationRunEnabled(false);
    }

    @Override
    public void loadingDone() {
        setSimulationRunEnabled(true);
    }

    /* ISimulationListener */

    @Override
    public void simulationStarted(PhspSimulator simulator) {
        setSimulationRunEnabled(false);
    }

    @Override
    public void simulationDone() {
        setSimulationRunEnabled(true);
    }
}
