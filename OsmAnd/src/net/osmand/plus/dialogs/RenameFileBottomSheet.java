package net.osmand.plus.dialogs;

import static net.osmand.plus.utils.FileUtils.ILLEGAL_FILE_NAME_CHARACTERS;
import static net.osmand.plus.utils.FileUtils.renameFile;
import static net.osmand.plus.utils.FileUtils.renameGpxFile;
import static net.osmand.plus.utils.FileUtils.renameSQLiteFile;

import android.content.res.ColorStateList;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import net.osmand.IndexConstants;
import net.osmand.PlatformUtil;
import net.osmand.plus.OsmandApplication;
import net.osmand.plus.R;
import net.osmand.plus.base.MenuBottomSheetDialogFragment;
import net.osmand.plus.base.bottomsheetmenu.BaseBottomSheetItem;
import net.osmand.plus.base.bottomsheetmenu.simpleitems.TitleItem;
import net.osmand.plus.resources.SQLiteTileSource;
import net.osmand.plus.utils.AndroidUtils;
import net.osmand.plus.utils.ColorUtilities;
import net.osmand.plus.utils.FileUtils.RenameCallback;
import net.osmand.plus.utils.UiUtilities;
import net.osmand.util.Algorithms;

import org.apache.commons.logging.Log;

import java.io.File;

public class RenameFileBottomSheet extends MenuBottomSheetDialogFragment {

	private static final Log LOG = PlatformUtil.getLog(RenameFileBottomSheet.class);
	private static final String TAG = RenameFileBottomSheet.class.getName();
	private static final String SOURCE_FILE_NAME_KEY = "source_file_name_key";
	private static final String SELECTED_FILE_NAME_KEY = "selected_file_name_key";

	private OsmandApplication app;

	private TextInputLayout nameTextBox;
	private TextInputEditText editText;

	private File file;
	private String selectedFileName;

	@Override
	public void createMenuItems(Bundle savedInstanceState) {
		app = requiredMyApplication();
		if (savedInstanceState != null) {
			String path = savedInstanceState.getString(SOURCE_FILE_NAME_KEY);
			if (!Algorithms.isEmpty(path)) {
				file = new File(path);
			}
			selectedFileName = savedInstanceState.getString(SELECTED_FILE_NAME_KEY);
		} else {
			selectedFileName = Algorithms.getFileNameWithoutExtension(file);
		}
		items.add(new TitleItem(getString(R.string.shared_string_rename)));

		View mainView = UiUtilities.getInflater(app, nightMode).inflate(R.layout.track_name_edit_text, null);
		nameTextBox = setupTextBox(mainView);
		editText = setupEditText(mainView);
		AndroidUtils.softKeyboardDelayed(getActivity(), editText);

		BaseBottomSheetItem editFolderName = new BaseBottomSheetItem.Builder()
				.setCustomView(mainView)
				.create();
		items.add(editFolderName);
	}

	private TextInputLayout setupTextBox(View mainView) {
		TextInputLayout nameTextBox = mainView.findViewById(R.id.name_text_box);
		int backgroundId = nightMode ? R.color.list_background_color_dark : R.color.activity_background_color_light;
		nameTextBox.setBoxBackgroundColorResource(backgroundId);
		nameTextBox.setHint(AndroidUtils.addColon(app, R.string.shared_string_name));
		ColorStateList colorStateList = ColorStateList.valueOf(ColorUtilities.getSecondaryTextColor(app, nightMode));
		nameTextBox.setDefaultHintTextColor(colorStateList);
		return nameTextBox;
	}

	private TextInputEditText setupEditText(View mainView) {
		TextInputEditText editText = mainView.findViewById(R.id.name_edit_text);
		editText.setText(selectedFileName);
		editText.requestFocus();
		editText.addTextChangedListener(new TextWatcher() {
			@Override
			public void beforeTextChanged(CharSequence s, int start, int count, int after) {
			}

			@Override
			public void onTextChanged(CharSequence s, int start, int before, int count) {
			}

			@Override
			public void afterTextChanged(Editable s) {
				updateFileName(s.toString());
			}
		});
		return editText;
	}

	private void updateFileName(String name) {
		if (Algorithms.isBlank(name)) {
			nameTextBox.setError(getString(R.string.empty_filename));
		} else if (ILLEGAL_FILE_NAME_CHARACTERS.matcher(name).find()) {
			nameTextBox.setError(getString(R.string.file_name_containes_illegal_char));
		} else {
			selectedFileName = name;
			nameTextBox.setError(null);
		}
		updateBottomButtons();
	}

	@Override
	protected boolean isRightBottomButtonEnabled() {
		return nameTextBox.getError() == null;
	}

	@Override
	public void onSaveInstanceState(@NonNull Bundle outState) {
		outState.putString(SOURCE_FILE_NAME_KEY, file.getAbsolutePath());
		outState.putString(SELECTED_FILE_NAME_KEY, selectedFileName);
		super.onSaveInstanceState(outState);
	}

	@Override
	protected void onRightBottomButtonClick() {
		FragmentActivity activity = getActivity();
		if (activity != null) {
			AndroidUtils.hideSoftKeyboard(activity, editText);
		}

		int idxOfLastDot = file.getName().lastIndexOf('.');
		String extension = idxOfLastDot == -1 ? "" : file.getName().substring(idxOfLastDot).trim();
		String newValidName = selectedFileName.trim();
		if (newValidName.endsWith(extension)) {
			newValidName = newValidName.substring(0, newValidName.lastIndexOf(extension)).trim();
		}

		File dest;
		if (SQLiteTileSource.EXT.equals(extension)) {
			dest = renameSQLiteFile(app, file, newValidName + extension, null);
		} else if (IndexConstants.GPX_FILE_EXT.equals(extension)) {
			dest = renameGpxFile(app, file, newValidName + extension, false, null);
		} else {
			dest = renameFile(app, file, newValidName + extension, false, null);
		}
		if (dest != null) {
			Fragment fragment = getTargetFragment();
			if (fragment instanceof RenameCallback) {
				RenameCallback listener = (RenameCallback) fragment;
				listener.renamedTo(dest);
			}
			dismiss();
		}
	}

	@Override
	protected int getDismissButtonTextId() {
		return R.string.shared_string_cancel;
	}

	@Override
	protected int getRightBottomButtonTextId() {
		return R.string.shared_string_save;
	}

	public static void showInstance(@NonNull FragmentManager fragmentManager, @Nullable Fragment target,
	                                @NonNull File file, boolean usedOnMap) {
		if (file.exists() && !fragmentManager.isStateSaved()
				&& fragmentManager.findFragmentByTag(TAG) == null) {
			RenameFileBottomSheet fragment = new RenameFileBottomSheet();
			fragment.file = file;
			fragment.setUsedOnMap(usedOnMap);
			fragment.setTargetFragment(target, 0);
			fragment.show(fragmentManager, TAG);
		}
	}
}