#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QStatusBar>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QFileInfo>
#include <QDir>

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

static QString findActiveRecordingFile()
{
	// 1. Intentamos sacar la ruta directa del output (incluyendo señales internas de Hybrid MP4)
	obs_output_t *output = obs_frontend_get_recording_output();
	if (output) {
		obs_data_t *settings = obs_output_get_settings(output);
		const char *path = obs_data_get_string(settings, "path");
		if (!path || !*path)
			path = obs_data_get_string(settings, "url");
		QString result = path ? QString::fromUtf8(path) : "";
		obs_data_release(settings);
		obs_output_release(output);
		if (!result.isEmpty() && QFileInfo::exists(result))
			return result;
	}

	// 2. Metodo infalible para OBS 32 Hybrid MP4: leemos la carpeta de grabacion actual
	char *recDir = obs_frontend_get_current_record_output_path();
	if (recDir) {
		QDir dir(QString::fromUtf8(recDir));
		bfree(recDir);
		QFileInfoList list = dir.entryInfoList(
			QStringList() << "*.mp4" << "*.mkv" << "*.mov" << "*.flv" << "*.ts", QDir::Files, QDir::Time);
		if (!list.isEmpty()) {
			return list.first().absoluteFilePath();
		}
	}
	return "";
}

static void initStatusBarWidget()
{
	if (filesizeLabel != nullptr)
		return;

	auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!mainWindow)
		return;

	filesizeLabel = new QLabel(" 📦 Peso: 0.0 MB ", mainWindow);
	filesizeLabel->setStyleSheet(
		"color: #ffffff; background-color: #2b2e38; border-radius: 4px; font-weight: bold; padding: 2px 8px; margin: 0 4px;");

	// Buscamos la barra de estado real de OBS 32 o el QStatusBar clasico
	QStatusBar *statusBar = mainWindow->findChild<QStatusBar *>();
	if (statusBar) {
		statusBar->addPermanentWidget(filesizeLabel);
		blog(LOG_INFO, "[obs-videosizer] Inyectado en QStatusBar exitosamente!");
	} else {
		QWidget *obsStatusBar = mainWindow->findChild<QWidget *>("statusbar");
		if (obsStatusBar && obsStatusBar->layout()) {
			obsStatusBar->layout()->addWidget(filesizeLabel);
			blog(LOG_INFO, "[obs-videosizer] Inyectado en OBS 32 statusbar layout!");
		} else {
			mainWindow->statusBar()->addPermanentWidget(filesizeLabel);
			blog(LOG_INFO, "[obs-videosizer] Inyectado en fallback statusBar!");
		}
	}

	updateTimer = new QTimer(mainWindow);
	QObject::connect(updateTimer, &QTimer::timeout, []() {
		if (currentFilePath.isEmpty()) {
			currentFilePath = findActiveRecordingFile();
		}

		if (!currentFilePath.isEmpty()) {
			QFileInfo fileInfo(currentFilePath);
			fileInfo.refresh(); // Fuerza a Windows a leer el peso real del disco sin cache
			if (fileInfo.exists() && filesizeLabel) {
				filesizeLabel->setText(" 🔴 REC: " + formatSize(fileInfo.size()) + " ");
			}
		}
	});
}

static void onFrontendEvent(enum obs_frontend_event event, void * /* private_data */)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		initStatusBarWidget();
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STARTED) {
		initStatusBarWidget();
		currentFilePath.clear();
		if (filesizeLabel) {
			filesizeLabel->setStyleSheet(
				"color: #ffffff; background-color: #aa2222; border-radius: 4px; font-weight: bold; padding: 2px 8px; margin: 0 4px;");
			filesizeLabel->setText(" 🔴 REC: 0.0 KB ");
		}
		if (updateTimer)
			updateTimer->start(500); // Actualiza cada medio segundo (500ms)
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPED) {
		if (updateTimer)
			updateTimer->stop();
		if (!currentFilePath.isEmpty() && filesizeLabel) {
			QFileInfo fileInfo(currentFilePath);
			fileInfo.refresh();
			if (fileInfo.exists()) {
				filesizeLabel->setText(" 📦 Final: " + formatSize(fileInfo.size()) + " ");
			}
		}
		if (filesizeLabel) {
			filesizeLabel->setStyleSheet(
				"color: #ffffff; background-color: #2b2e38; border-radius: 4px; font-weight: bold; padding: 2px 8px; margin: 0 4px;");
		}
		currentFilePath.clear();
	}
}

bool obs_module_load(void)
{
	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	blog(LOG_INFO, "[obs-videosizer] EDICION KIWII CARGADA (Esperando interfaz de OBS 32)...");
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
}