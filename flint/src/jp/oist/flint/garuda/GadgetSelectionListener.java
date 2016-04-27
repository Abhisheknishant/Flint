/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 et: */
package jp.oist.flint.garuda;

import jp.sbi.garuda.backend.POJOs.CompatibleGadgetDetails;
import jp.sbi.garuda.backend.exception.NoFileToSendException;
import java.util.List;
import javax.swing.JFrame;
import javax.swing.JOptionPane;
import javax.swing.ListSelectionModel;
import javax.swing.SwingUtilities;
import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;

class GadgetSelectionListener implements ListSelectionListener {

    final GadgetDialog mDialog;
    final JFrame mFrame;
    final List<CompatibleGadgetDetails> mGadgets;

    public GadgetSelectionListener(GadgetDialog dialog,
                                   JFrame frame,
                                   List<CompatibleGadgetDetails> gadgets) {
        mDialog = dialog;
        mFrame = frame;
        mGadgets = gadgets;
    }

    @Override
    public void valueChanged(ListSelectionEvent event) {
        if (event.getValueIsAdjusting()) return; // ignore it
        ListSelectionModel lsm = (ListSelectionModel)event.getSource();
        if (lsm.isSelectionEmpty()) return; // ignore it
        int minIndex = lsm.getMinSelectionIndex();
        int maxIndex = lsm.getMaxSelectionIndex();
        for (int i = minIndex; i <= maxIndex; i++) {
            if (lsm.isSelectedIndex(i)) {
                mDialog.dispose();
                try {
                    GarudaClient.commitSendFile(mGadgets.get(i), mFrame);
                } catch (NoFileToSendException e) {
                    final String message = e.getMessage();
                    SwingUtilities.invokeLater(new Runnable() {
                        @Override
                        public void run() {
                            JOptionPane.showMessageDialog(mFrame,
                                                          message,
                                                          "Error on sending file",
                                                          JOptionPane.ERROR_MESSAGE);
                        }
                    });
                    mFrame.setEnabled(true);
                }
                return;
            }
        }
    }
}
