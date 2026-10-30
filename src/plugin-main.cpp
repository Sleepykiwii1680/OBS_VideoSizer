#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
#include <QFileInfo>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-videosizer", "en-US")

static QLabel *filesizeLabel = nullptr;
static QTimer *updateTimer = nullptr;
static QString currentFilePath = "";

static QString formatSize(qint64 bytes)
{
	if (bytes < 1024)
		return QString::number(bytes) + " B";
	if (bytes < 1024 * 1024)
		return QString::number(bytes / 1024.0, 'f', 1) + " KB";
	if (bytes < 1024LL * 1024 * 1024)
		return QString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " MB";
	return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

static void initStatusBarWidget()
{
	if (filesizeLabel != nullptr)
		return;

	auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow)
		return;

	QStatusBar *statusBar = mainWindow->statusBar();
	if (!statusBar)
		return;

	filesizeLabel = new QLabel(" 📦 Peso: 0.0 MB ", statusBar);
	filesizeLabel->setStyleSheet("color: #cccccc; font-weight: bold; padding: 0 8px;");
	statusBar->addPermanentWidget(filesizeLabel);

	updateTimer = new QTimer(mainWindow);
	QObject::connect(updateTimer, &QTimer::timeout, []() {
		if (currentFilePath.isEmpty()) {
			obs_output_t *output = obs_frontend_get_recording_output();
			if (output) {
				obs_data_t *settings = obs_output_get_settings(output);
				const char *path = obs_data_get_string(settings, "path");
				if (!path || !*path)
					path = obs_data_get_string(settings, "url");
				if (path)
					currentFilePath = QString::fromUtf8(path);
				obs_data_release(settings);
				obs_output_release(output);
			}
		}

		if (!currentFilePath.isEmpty()) {
			QFileInfo fileInfo(currentFilePath);
			if (fileInfo.exists() && filesizeLabel) {
				filesizeLabel->setText(" 🔴 REC: " + formatSize(fileInfo.size()) + " ");
			}
		}
	});

	blog(LOG_INFO, "[obs-videosizer] Widget inyectado en la barra de estado con exito!");
}

static void onFrontendEvent(enum obs_frontend_event event, void * /* private_data */)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		initStatusBarWidget();
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STARTED) {
		initStatusBarWidget();
		currentFilePath.clear();
		if (filesizeLabel) {
			filesizeLabel->setStyleSheet("color: #ff5555; font-weight: bold; padding: 0 8px;");
			filesizeLabel->setText(" 🔴 REC: 0.0 KB ");
		}
		if (updateTimer)
			updateTimer->start(1000);
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPED) {
		if (updateTimer)
			updateTimer->stop();
		if (!currentFilePath.isEmpty() && filesizeLabel) {
			QFileInfo fileInfo(currentFilePath);
			if (fileInfo.exists()) {
				filesizeLabel->setText(" 📦 Ultimo: " + formatSize(fileInfo.size()) + " ");
			}
		}
		if (filesizeLabel) {
			filesizeLabel->setStyleSheet("color: #cccccc; font-weight: bold; padding: 0 8px;");
		}
		currentFilePath.clear();
	}
}

bool obs_module_load(void)
{
	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	blog(LOG_INFO, "[obs-videosizer] Modulo cargado, esperando a la interfaz...");
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
}