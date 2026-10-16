import UIKit
import UniformTypeIdentifiers

/// Imports a user-selected 3DS image into the app sandbox and starts Zakuro.
final class ImportViewController: UIViewController, UIDocumentPickerDelegate {
    private let rightsSwitch = UISwitch()
    private let statusLabel = UILabel()
    private let runtimeButton = UIButton(type: .system)
    private let controlsButton = UIButton(type: .system)

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        title = "Yo-kai Watch 2"

        let titleLabel = UILabel()
        titleLabel.text = "Importar una copia autorizada"
        titleLabel.font = .preferredFont(forTextStyle: .title2)
        titleLabel.textAlignment = .center

        let rightsLabel = UILabel()
        rightsLabel.text = "Tengo derecho a usar esta copia en este dispositivo"
        rightsLabel.numberOfLines = 0

        let rightsRow = UIStackView(arrangedSubviews: [rightsLabel, rightsSwitch])
        rightsRow.axis = .horizontal
        rightsRow.alignment = .center
        rightsRow.spacing = 12

        let button = UIButton(type: .system)
        button.setTitle("Seleccionar ROM", for: .normal)
        button.addTarget(self, action: #selector(selectRom), for: .touchUpInside)

        runtimeButton.setTitle("Iniciar Yo-kai Watch 2", for: .normal)
        runtimeButton.addTarget(self, action: #selector(openRuntime), for: .touchUpInside)
        runtimeButton.isHidden = true

        controlsButton.setTitle("Configurar controles táctiles", for: .normal)
        controlsButton.addTarget(self, action: #selector(openControlSettings), for: .touchUpInside)

        statusLabel.text = "Selecciona una copia local autorizada para comenzar."
        statusLabel.numberOfLines = 0
        statusLabel.textAlignment = .center

        let stack = UIStackView(arrangedSubviews: [titleLabel, rightsRow, button, runtimeButton, controlsButton, statusLabel])
        stack.axis = .vertical
        stack.spacing = 20
        stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 24),
            stack.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -24),
            stack.centerYAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerYAnchor),
        ])

        if let romURL = try? romURL(), FileManager.default.fileExists(atPath: romURL.path) {
            runtimeButton.isHidden = false
            statusLabel.text = "ROM guardada en este iPhone. Zakuro la abrirá desde el almacenamiento privado."
            if let dataURL = try? dataDirectoryURL(),
               let previousState = try? String(
                   contentsOf: dataURL.appendingPathComponent("zakuro-startup-state.txt"),
                   encoding: .utf8
               ),
               !previousState.isEmpty,
               previousState != "first-frame.success" {
                statusLabel.text = "Último arranque interrumpido: \(previousState). Si vuelve a fallar, envíame este estado."
            }
        }
    }

    @objc private func openControlSettings() {
        let controller = ZakuroViewController(configurationOnly: true)
        navigationController?.pushViewController(controller, animated: true)
    }

    @objc private func openRuntime() {
        guard let romURL = try? romURL(),
              let dataURL = try? dataDirectoryURL(),
              FileManager.default.fileExists(atPath: romURL.path) else {
            statusLabel.text = "Selecciona primero tu copia autorizada de la ROM."
            return
        }
        let controller = ZakuroViewController(romURL: romURL, dataURL: dataURL)
        if let navigationController {
            navigationController.pushViewController(controller, animated: true)
        } else {
            present(controller, animated: true)
        }
    }

    @objc private func selectRom() {
        guard rightsSwitch.isOn else {
            statusLabel.text = "Confirma primero que tienes derecho a usar la copia."
            return
        }
        let romType = UTType(filenameExtension: "3ds") ?? .data
        let picker = UIDocumentPickerViewController(
            // Some Files providers do not map .3ds to public.data. Including
            // public.item keeps the picker able to select the user's dump.
            forOpeningContentTypes: [romType, .data, .item],
            asCopy: true
        )
        picker.delegate = self
        picker.allowsMultipleSelection = false
        present(picker, animated: true)
    }

    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        guard let url = urls.first else {
            statusLabel.text = "No se seleccionó ningún archivo."
            return
        }
        let hasSecurityScope = url.startAccessingSecurityScopedResource()
        do {
            let attributes = try FileManager.default.attributesOfItem(atPath: url.path)
            let size = (attributes[.size] as? NSNumber)?.uint64Value ?? 0
            guard size >= 0x200 else {
                if hasSecurityScope { url.stopAccessingSecurityScopedResource() }
                statusLabel.text = "La imagen es demasiado pequeña."
                return
            }
            let handle = try FileHandle(forReadingFrom: url)
            defer { try? handle.close() }
            let header = try handle.read(upToCount: 0x200) ?? Data()
            guard header.count == 0x200,
                  header.subdata(in: 0x100..<0x104) == Data("NCSD".utf8) else {
                if hasSecurityScope { url.stopAccessingSecurityScopedResource() }
                statusLabel.text = "La imagen no es un dump NCSD/3DS válido."
                return
            }
            try? handle.close()
            statusLabel.text = "NCSD válido. Copiando la ROM al almacenamiento privado del iPhone…"
            let destinationURL = try romURL()
            let directoryURL = try dataDirectoryURL()
            DispatchQueue.global(qos: .userInitiated).async {
                var copyError: Error?
                do {
                    try FileManager.default.createDirectory(
                        at: directoryURL,
                        withIntermediateDirectories: true
                    )
                    let temporaryURL = destinationURL.appendingPathExtension("part")
                    try? FileManager.default.removeItem(at: temporaryURL)
                    try FileManager.default.copyItem(at: url, to: temporaryURL)
                    if FileManager.default.fileExists(atPath: destinationURL.path) {
                        try FileManager.default.removeItem(at: destinationURL)
                    }
                    try FileManager.default.moveItem(at: temporaryURL, to: destinationURL)
                } catch {
                    copyError = error
                }
                DispatchQueue.main.async {
                    if hasSecurityScope { url.stopAccessingSecurityScopedResource() }
                    if copyError == nil {
                        self.runtimeButton.isHidden = false
                        self.statusLabel.text = "ROM copiada al almacenamiento privado. Pulsa Iniciar para abrirla con Zakuro."
                    } else {
                        self.statusLabel.text = "No se pudo copiar la ROM: \(copyError!.localizedDescription)"
                    }
                }
            }
        } catch {
            if hasSecurityScope { url.stopAccessingSecurityScopedResource() }
            statusLabel.text = "No se pudo leer la imagen: \(error.localizedDescription)"
        }
    }

    private func dataDirectoryURL() throws -> URL {
        try FileManager.default.url(
            for: .applicationSupportDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: true
        ).appendingPathComponent("ykw2-data", isDirectory: true)
    }

    private func romURL() throws -> URL {
        try dataDirectoryURL().appendingPathComponent("authorized-rom.3ds")
    }
}
