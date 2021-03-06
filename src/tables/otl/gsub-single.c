#include "gsub-single.h"
void caryll_delete_gsub_single(otl_lookup *lookup) {
	if (lookup) {
		if (lookup->subtables) {
			for (uint16_t j = 0; j < lookup->subtableCount; j++)
				if (lookup->subtables[j]) {
					caryll_delete_coverage(lookup->subtables[j]->gsub_single.from);
					caryll_delete_coverage(lookup->subtables[j]->gsub_single.to);
					free(lookup->subtables[j]);
				}
			free(lookup->subtables);
		}
		FREE(lookup);
	}
}

otl_subtable *caryll_read_gsub_single(font_file_pointer data, uint32_t tableLength,
                                      uint32_t subtableOffset) {
	otl_subtable *subtable;
	NEW(subtable);
	if (tableLength < subtableOffset + 6) goto FAIL;
	uint16_t subtableFormat = read_16u(data + subtableOffset);
	otl_coverage *from = caryll_read_coverage(data, tableLength,
	                                          subtableOffset + read_16u(data + subtableOffset + 2));
	subtable->gsub_single.from = from;
	if (!from || from->numGlyphs == 0) goto FAIL;

	if (subtableFormat == 1) {
		otl_coverage *to;
		NEW(to);
		to->numGlyphs = from->numGlyphs;
		NEW_N(to->glyphs, to->numGlyphs);

		uint16_t delta = read_16u(data + subtableOffset + 4);
		for (uint16_t j = 0; j < from->numGlyphs; j++) {
			to->glyphs[j].gid = from->glyphs[j].gid + delta;
			to->glyphs[j].name = NULL;
		}
		subtable->gsub_single.to = to;
	} else {
		uint16_t toglyphs = read_16u(data + subtableOffset + 4);
		if (tableLength < subtableOffset + 6 + toglyphs * 2 || toglyphs != from->numGlyphs)
			goto FAIL;
		otl_coverage *to;
		NEW(to);
		to->numGlyphs = toglyphs;
		NEW_N(to->glyphs, to->numGlyphs);

		for (uint16_t j = 0; j < to->numGlyphs; j++) {
			to->glyphs[j].gid = read_16u(data + subtableOffset + 6 + j * 2);
			to->glyphs[j].name = NULL;
		}
		subtable->gsub_single.to = to;
	}
	goto OK;
FAIL:
	if (subtable->gsub_single.from) caryll_delete_coverage(subtable->gsub_single.from);
	if (subtable->gsub_single.to) caryll_delete_coverage(subtable->gsub_single.to);
	subtable = NULL;
OK:
	return subtable;
}

json_value *caryll_gsub_single_to_json(otl_subtable *_subtable) {
	subtable_gsub_single *subtable = &(_subtable->gsub_single);
	json_value *st = json_object_new(subtable->from->numGlyphs);
	for (uint16_t j = 0; j < subtable->from->numGlyphs && j < subtable->from->numGlyphs; j++) {
		json_object_push(st, subtable->from->glyphs[j].name,
		                 json_string_new(subtable->to->glyphs[j].name));
	}
	return st;
}

otl_subtable *caryll_gsub_single_from_json(json_value *_subtable) {
	otl_subtable *_st;
	NEW(_st);
	subtable_gsub_single *subtable = &(_st->gsub_single);
	NEW(subtable->from);
	NEW(subtable->to);
	subtable->from->numGlyphs = subtable->to->numGlyphs = _subtable->u.object.length;
	NEW_N(subtable->from->glyphs, subtable->from->numGlyphs);
	NEW_N(subtable->to->glyphs, subtable->to->numGlyphs);
	uint16_t jj = 0;
	for (uint16_t j = 0; j < _subtable->u.object.length; j++) {
		if (_subtable->u.object.values[j].value &&
		    _subtable->u.object.values[j].value->type == json_string) {
			subtable->from->glyphs[jj].name = sdsnewlen(_subtable->u.object.values[j].name,
			                                            _subtable->u.object.values[j].name_length);
			subtable->to->glyphs[jj].name =
			    sdsnewlen(_subtable->u.object.values[j].value->u.string.ptr,
			              _subtable->u.object.values[j].value->u.string.length);
			jj++;
		}
	}
	subtable->from->numGlyphs = subtable->to->numGlyphs = jj;
	return _st;
};

caryll_buffer *caryll_write_gsub_single_subtable(otl_subtable *_subtable) {
	caryll_buffer *bufst = bufnew();
	subtable_gsub_single *subtable = &(_subtable->gsub_single);
	bool isConstantDifference = true;
	if (subtable->from->numGlyphs > 1) {
		int32_t difference = subtable->to->glyphs[0].gid - subtable->from->glyphs[0].gid;
		for (uint16_t j = 1; j < subtable->from->numGlyphs; j++) {
			isConstantDifference =
			    isConstantDifference &&
			    ((subtable->to->glyphs[j].gid - subtable->from->glyphs[j].gid) == difference);
		}
	}
	if (isConstantDifference && subtable->from->numGlyphs > 0) {
		bufwrite16b(bufst, 1);
		bufwrite16b(bufst, 6);
		bufwrite16b(bufst, subtable->to->glyphs[0].gid - subtable->from->glyphs[0].gid);
		bufwrite_bufdel(bufst, caryll_write_coverage(subtable->from));
	} else {
		bufwrite16b(bufst, 2);
		bufwrite16b(bufst, 6 + subtable->to->numGlyphs * 2);
		bufwrite16b(bufst, subtable->to->numGlyphs);
		for (uint16_t k = 0; k < subtable->to->numGlyphs; k++) {
			bufwrite16b(bufst, subtable->to->glyphs[k].gid);
		}
		bufwrite_bufdel(bufst, caryll_write_coverage(subtable->from));
	}
	return bufst;
}
