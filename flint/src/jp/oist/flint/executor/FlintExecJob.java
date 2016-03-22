/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 et: */
package jp.oist.flint.executor;

import jp.physiome.Cli.ExecOption;
import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import jp.oist.flint.component.Component;


public class FlintExecJob implements IJob<Boolean> {

    private final ExecOption mOption;

    private final ProcessBuilder mProcessBuilder;

    private final Process mProcess;

    public FlintExecJob(ExecOption option, File workingDir) throws IOException {
        mOption = option;
        mProcessBuilder = new ProcessBuilder(Component.getFlintExecCommand());
        Component.setUpEnvironment(mProcessBuilder);
        mProcessBuilder.directory(workingDir);
        mProcessBuilder.redirectErrorStream(true);
        mProcess = mProcessBuilder.start();
    }

    @Override
    public Boolean call() throws IOException, InterruptedException, JobException {
        try (OutputStream os = mProcess.getOutputStream()) {
            mOption.writeTo(os);
        }
        int r = mProcess.waitFor();
        if (r == 0) return true;

        throw new JobException(this);
    }

    @Override
    public Process getProcess() {
        return mProcess;
    }
}
