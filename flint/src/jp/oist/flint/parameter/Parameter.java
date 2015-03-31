/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 et: */
package jp.oist.flint.parameter;

import jp.physiome.Lo.Column;
import jp.physiome.Lo.Type;

public class Parameter {

    private final Column mColumn;
    private String mValue;

    Parameter(Column column, double value) {
        mColumn = column;
        mValue  = String.valueOf(value);
    }

    /**
     * 
     * @return module-name if phml-file, other null
     *          
     */
    public String getTrackName() {
        return mColumn.getTrackName();
    }


    /**
     * 
     * @return module-id if phml-file, other null
     */
    public String getUuid () {
        return mColumn.getUuid().toStringUtf8();
    }

    /**
     * 
     * @return physical-quantity-name if phml, 
     *         species-id if sbml other null
     */
    public String getName() {
        return mColumn.getName();
    }

    public Type getType() {
        return mColumn.getType();
    }

    /**
     * 
     * @return physical-quantity-id if phml other null
     */
    public String getId() {
        return String.valueOf(mColumn.getId());
    }

    public String getValue() {
        return mValue;
    }

    int getPosition () {
        return mColumn.getPosition();
    }

    public void setValue (String value) {
        mValue = value;
    }
}
