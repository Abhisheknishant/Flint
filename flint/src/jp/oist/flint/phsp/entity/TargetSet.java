/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 et: */
package jp.oist.flint.phsp.entity;

import jp.oist.flint.textformula.TextFormula2MathML;
import jp.oist.flint.textformula.analyzer.ParseException;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Set;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.TransformerException;

public class TargetSet {

    private final ArrayList<Target> mTargets = new ArrayList<>();

    public Target[] getTargets () {
        return (Target[]) mTargets.toArray(new Target[mTargets.size()]);
    }

    public void add (TargetSet.Target target) {
        mTargets.add(target);
    }

    public int size() {
        return mTargets.size();
    }

    public Set<String> findNames()
        throws ParseException,
               ParserConfigurationException,
               TransformerException {
        HashSet<String> names = new HashSet<>();
        TextFormula2MathML s2mathml = new TextFormula2MathML();
        for (Target t : mTargets) {
            s2mathml.parse(new StringReader(t.getValue()));
            names.addAll(s2mathml.findNames());
        }
        return names;
    }

    public static class Target {
        private String mModuleId;

        private String mModuleName;

        private String mPhysicalQuantityId;

        private String mPhysicalQuantityName;

        private String mId;

        private String mValue;

        public Target () {
            mModuleId = "";
            mModuleName = "";
            mPhysicalQuantityId = "";
            mPhysicalQuantityName = "";
            mId = "";
            mValue = "";
        }

        public String getModuleId() {
            return mModuleId;
        }

        public void setModuleId (String moduleId) {
            mModuleId = moduleId;
        }

        public String getModuleName () {
            return mModuleName;
        }

        public void setModuleName (String moduleName) {
            mModuleName = moduleName;
        }

        public String getPhysicalQuantityId() { 
            return mPhysicalQuantityId;
        }

        public void setPhysicalQuantityId (String pqId) {
            mPhysicalQuantityId = pqId;
        }

        public String getPhysicalQuantityName() { 
            return mPhysicalQuantityName;
        }

        public void setPhysicalQuantityName (String pqName) {
            mPhysicalQuantityName = pqName;
        }

        public String getId() {
            return mId;
        }

        public void setId (String sId) {
            mId = sId;
        }

        public String getValue() {
            return mValue;
        }

        public void setValue (String value) {
            mValue = value;
        }
    }
}
