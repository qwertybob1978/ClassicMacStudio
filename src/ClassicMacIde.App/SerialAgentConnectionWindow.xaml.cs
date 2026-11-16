using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows;
using Microsoft.Win32;
using ClassicMacIde.Core;

namespace ClassicMacIde.App;

public partial class SerialAgentConnectionWindow : Window
{
    private static readonly string HostDiagnosticsDirectory = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "ClassicMacStudio",
        "Logs");
    private static readonly string HostDiagnosticsPath = Path.Combine(
        HostDiagnosticsDirectory,
        "SerialAgentHost.log");

    private SerialPort? _serialPort;
    private CancellableSerialPortStream? _serialStream;
    private MacStudioAgentSession? _session;
    private CancellationTokenSource? _connectionCancellation;
    private CancellationTokenSource? _transferCancellation;
    private CancellationTokenSource? _commandCancellation;
    private CancellationTokenSource? _debugReceiveCancellation;
    private Task? _debugReceiveTask;
    private byte[]? _lastBuildId;
    private string[] _availableComPorts = [];
    private string _connectionStage = "idle";
    private readonly object _diagnosticsLock = new();
    private bool _isConnecting;
    private bool _isTransferring;
    private bool _isCommandInProgress;
    private bool _isApplicationRunning;
    private bool _closeAfterTransfer;
    private bool _closeAfterCommand;
    private bool _isClosing;

    public SerialAgentConnectionWindow()
    {
        InitializeComponent();
        LogHost("HOST_START", GetHostMachineDetails());
        RefreshPorts();
    }

    private void RefreshPortsButton_Click(object sender, RoutedEventArgs e) =>
        RefreshPorts();

    private void SaveHostLogButton_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new SaveFileDialog
        {
            AddExtension = true,
            DefaultExt = ".log",
            FileName = "ClassicMacStudio-SerialAgentHost.log",
            Filter = "Log files (*.log)|*.log|Text files (*.txt)|*.txt",
            OverwritePrompt = true,
            Title = "Save Serial Agent host diagnostics",
        };
        if (dialog.ShowDialog(this) != true)
        {
            return;
        }

        try
        {
            File.Copy(HostDiagnosticsPath, dialog.FileName, overwrite: true);
            SetStatus($"Saved host diagnostics to {dialog.FileName}.");
        }
        catch (Exception error) when (
            error is IOException or
                UnauthorizedAccessException or
                ArgumentException or
                NotSupportedException)
        {
            SetStatus($"Could not save host diagnostics: {error.Message}");
        }
    }

    private async void ConnectButton_Click(object sender, RoutedEventArgs e) =>
        await ConnectToAgentAsync();

    private async void DisconnectButton_Click(object sender, RoutedEventArgs e)
    {
        DisconnectButton.IsEnabled = false;
        try
        {
            await StopDebugReceiverAsync();
            if (_session?.State == MacStudioAgentConnectionState.Connected)
            {
                await _session.DisconnectAsync();
            }

            LogHost("DISCONNECT", "Host closed the Agent session.");
            SetStatus("Disconnected.");
        }
        catch (Exception error) when (
            error is IOException or
                InvalidOperationException or
                TimeoutException or
                UnauthorizedAccessException or
                Win32Exception)
        {
            LogHost("DISCONNECT_FAILURE", error.ToString());
            SetStatus($"Disconnect failed: {error.Message}");
        }
        finally
        {
            ResetConnection();
            UpdateControls();
        }
    }

    private async void SendPackageButton_Click(object sender, RoutedEventArgs e) =>
        await SendPackageAsync();

    private async void RunButton_Click(object sender, RoutedEventArgs e) =>
        await SendBuildCommandAsync(stop: false);

    private async void StopButton_Click(object sender, RoutedEventArgs e) =>
        await SendBuildCommandAsync(stop: true);

    private void CancelTransferButton_Click(object sender, RoutedEventArgs e)
    {
        _transferCancellation?.Cancel();
        SetStatus("Cancelling the active transfer...");
    }

    private async void SerialAgentConnectionWindow_Closing(
        object? sender,
        CancelEventArgs e)
    {
        if (_isTransferring)
        {
            e.Cancel = true;
            _closeAfterTransfer = true;
            _transferCancellation?.Cancel();
            SetStatus("Cancelling the active transfer before closing...");
            return;
        }

        if (_isCommandInProgress)
        {
            e.Cancel = true;
            _isClosing = true;
            _closeAfterCommand = true;
            _commandCancellation?.Cancel();
            SetStatus("Cancelling the active Agent command before closing...");
            return;
        }

        if (_debugReceiveTask is not null)
        {
            e.Cancel = true;
            _isClosing = true;
            await StopDebugReceiverAsync();
            ResetConnection();
            Close();
            return;
        }

        _isClosing = true;
        _connectionCancellation?.Cancel();
        ResetConnection();
    }

    private async Task SendPackageAsync()
    {
        if (_isTransferring ||
            _session?.State != MacStudioAgentConnectionState.Connected)
        {
            return;
        }

        var destinationName = DestinationNameTextBox.Text.Trim();
        var dialog = new OpenFileDialog
        {
            CheckFileExists = true,
            Filter = "MacBinary II packages (*.bin;*.macbin)|*.bin;*.macbin|All files (*.*)|*.*",
            Multiselect = false,
            Title = "Select a MacBinary II application package",
        };
        if (dialog.ShowDialog(this) != true)
        {
            return;
        }

        var session = _session;
        if (session?.State != MacStudioAgentConnectionState.Connected)
        {
            SetStatus("The Agent disconnected before the transfer could start.");
            return;
        }

        using var cancellation = new CancellationTokenSource();
        _transferCancellation = cancellation;
        _isTransferring = true;
        UpdateControls();
        SetStatus($"Sending {Path.GetFileName(dialog.FileName)} to {destinationName}...");
        LogHost(
            "PACKAGE_TRANSFER_BEGIN",
            $"File={Path.GetFileName(dialog.FileName)}; destination={destinationName}");

        try
        {
            await StopDebugReceiverAsync();
            var package = await session.PutAsync(
                dialog.FileName,
                destinationName,
                cancellation.Token);
            if (_isClosing)
            {
                return;
            }

            _lastBuildId = package.BuildId.ToArray();
            _isApplicationRunning = false;
            LogHost(
                "PACKAGE_TRANSFER_COMPLETE",
                string.Format(
                    CultureInfo.InvariantCulture,
                    "Length={0}; CRC32={1:X8}; BuildID={2}",
                    package.PackageLength,
                    package.PackageCrc32,
                    Convert.ToHexString(package.BuildId.Span)));
            SetStatus(
                $"Installed {destinationName} ({package.PackageLength:N0} bytes).");
            AgentDetailsText.Text += string.Format(
                CultureInfo.InvariantCulture,
                "\nLast package: CRC-32 {0:X8}; Build ID {1}.",
                package.PackageCrc32,
                Convert.ToHexString(package.BuildId.Span));
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested)
        {
            LogHost("PACKAGE_TRANSFER_CANCELLED", "The host cancelled the transfer.");
            SetStatus("Transfer cancelled.");
        }
        catch (Exception error) when (
            error is UnauthorizedAccessException or
                IOException or
                InvalidDataException or
                ArgumentException or
                InvalidOperationException or
                TimeoutException or
                Win32Exception)
        {
            LogHost("PACKAGE_TRANSFER_FAILURE", error.ToString());
            SetStatus($"Transfer failed: {error.Message}");
        }
        finally
        {
            if (ReferenceEquals(_transferCancellation, cancellation))
            {
                _transferCancellation = null;
            }

            _isTransferring = false;
            UpdateControls();
            if (!_isClosing &&
                _isApplicationRunning &&
                _session is { State: MacStudioAgentConnectionState.Connected } activeSession)
            {
                StartDebugReceiver(activeSession);
            }
            if (_closeAfterTransfer)
            {
                Close();
            }
        }
    }

    private async Task SendBuildCommandAsync(bool stop)
    {
        var session = _session;
        var buildId = _lastBuildId;
        if (_isTransferring ||
            _isCommandInProgress ||
            buildId is null ||
            session is null ||
            session.State != MacStudioAgentConnectionState.Connected)
        {
            return;
        }

        using var cancellation = new CancellationTokenSource();
        _commandCancellation = cancellation;
        _isCommandInProgress = true;
        UpdateControls();
        SetStatus(stop ? "Requesting the application to quit..." : "Launching the installed application...");
        LogHost("AGENT_COMMAND_BEGIN", stop ? "Command=STOP" : "Command=RUN");

        try
        {
            await StopDebugReceiverAsync();
            var resultCode = stop
                ? await session.StopAsync(buildId, cancellation.Token)
                : await session.RunAsync(buildId, cancellation.Token);
            LogHost(
                "AGENT_COMMAND_RESULT",
                string.Format(
                    CultureInfo.InvariantCulture,
                    "Command={0}; Result={1}",
                    stop ? "STOP" : "RUN",
                    resultCode));
            if (_isClosing)
            {
                return;
            }

            if (stop && resultCode == 0)
            {
                _isApplicationRunning = false;
                SetStatus("The Agent accepted the application quit request.");
            }
            else if (!stop && resultCode == 0)
            {
                _isApplicationRunning = true;
                SetStatus("The Agent launched the installed application.");
            }
            else
            {
                if (stop && resultCode == 1)
                {
                    _isApplicationRunning = false;
                }

                SetStatus(stop
                    ? $"Stop request failed (Agent result {resultCode})."
                    : $"Launch request failed (Agent result {resultCode}).");
            }
        }
        catch (OperationCanceledException) when (cancellation.IsCancellationRequested)
        {
            LogHost("AGENT_COMMAND_CANCELLED", stop ? "Command=STOP" : "Command=RUN");
            if (!_isClosing)
            {
                SetStatus("Agent command cancelled.");
            }
        }
        catch (Exception error) when (
            error is UnauthorizedAccessException or
                IOException or
                InvalidDataException or
                ArgumentException or
                InvalidOperationException or
                TimeoutException or
                Win32Exception)
        {
            if (!_isClosing)
            {
                SetStatus($"Agent command failed: {error.Message}");
            }
        }
        finally
        {
            if (ReferenceEquals(_commandCancellation, cancellation))
            {
                _commandCancellation = null;
            }

            _isCommandInProgress = false;
            UpdateControls();
            if (!_isClosing &&
                _session is { State: MacStudioAgentConnectionState.Connected } activeSession)
            {
                StartDebugReceiver(activeSession);
            }
            if (_closeAfterCommand)
            {
                Close();
            }
        }
    }

    private void StartDebugReceiver(MacStudioAgentSession session)
    {
        if (_debugReceiveTask is not null)
        {
            return;
        }

        var cancellation = new CancellationTokenSource();
        _debugReceiveCancellation = cancellation;
        _debugReceiveTask = ReceiveDebugRecordsAsync(session, cancellation.Token);
    }

    private async Task StopDebugReceiverAsync()
    {
        var receiveTask = _debugReceiveTask;
        if (receiveTask is null)
        {
            return;
        }

        var cancellation = _debugReceiveCancellation;
        _debugReceiveTask = null;
        _debugReceiveCancellation = null;
        if (cancellation is null)
        {
            await receiveTask;
            return;
        }

        cancellation.Cancel();
        try
        {
            await receiveTask;
        }
        finally
        {
            cancellation.Dispose();
        }
    }

    private async Task ReceiveDebugRecordsAsync(
        MacStudioAgentSession session,
        CancellationToken cancellationToken)
    {
        try
        {
            while (true)
            {
                var frame = await session.ReceiveAsync(cancellationToken);
                if (frame.MessageType != (byte)MacStudioAgentMessageType.DebugRecord)
                {
                    SetStatus($"Received unexpected Agent message 0x{frame.MessageType:X2}.");
                    continue;
                }

                var record = MacStudioAgentDebugRecordCodec.Decode(frame);
                LogHost(
                    "AGENT_DEBUG_RECORD",
                    string.Format(
                        CultureInfo.InvariantCulture,
                        "Sequence={0}; Severity={1}; Length={2}",
                        record.Sequence,
                        record.Severity,
                        record.Message.Length));
                await session.SendAsync(
                    MacStudioAgentDebugRecordCodec.CreateAcknowledgment(
                        session.SessionId,
                        record.Sequence,
                        0),
                    cancellationToken);
                AppendDebugRecord(record);
            }
        }
        catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
        {
            LogHost("AGENT_DEBUG_RECEIVE_CANCELLED", "CancellationToken requested");
        }
        catch (Exception error) when (
            error is IOException or
                InvalidOperationException or
                UnauthorizedAccessException)
        {
            if (!_isClosing)
            {
                SetStatus($"Debug receive failed: {error.Message}");
            }
        }
    }

    private void AppendDebugRecord(MacStudioAgentDebugRecord record)
    {
        const int maximumLogCharacters = 16_384;
        var entry = string.Format(
            CultureInfo.InvariantCulture,
            "[{0:D8}] severity {1}: {2}{3}",
            record.Sequence,
            record.Severity,
            record.Message,
            Environment.NewLine);
        var text = DebugLogTextBox.Text + entry;
        if (text.Length > maximumLogCharacters)
        {
            var start = text.IndexOf(
                '\n',
                text.Length - maximumLogCharacters);
            text = start >= 0
                ? text[(start + 1)..]
                : text[^maximumLogCharacters..];
        }

        DebugLogTextBox.Text = text;
        DebugLogTextBox.CaretIndex = text.Length;
        DebugLogTextBox.ScrollToEnd();
    }

    private async Task ConnectToAgentAsync()
    {
        if (_isConnecting || _session is not null)
        {
            return;
        }

        int baudRate;
        try
        {
            baudRate = GetSelectedBaudRate();
        }
        catch (InvalidOperationException error)
        {
            SetStatus(error.Message);
            return;
        }

        if (ComPortComboBox.SelectedItem is not string portName)
        {
            SetStatus("Select an available PC COM port.");
            return;
        }

        _isConnecting = true;
        using var cancellation = new CancellationTokenSource();
        _connectionCancellation = cancellation;
        UpdateControls();
        SetStatus($"Connecting to {portName} at {baudRate:N0} baud...");
        AgentDetailsText.Text = "Waiting for the Agent handshake.";
        _connectionStage = "create SerialPort";
        LogHost(
            "CONNECT_BEGIN",
            $"COM={portName}; Baud={baudRate}; Parity=None; DataBits=8; StopBits=One; Handshake=None");

        try
        {
            var port = new SerialPort(
                portName,
                baudRate,
                Parity.None,
                8,
                StopBits.One)
            {
                Handshake = Handshake.None,
                ReadTimeout = 1000,
                WriteTimeout = 1000,
            };
            _serialPort = port;
            _connectionStage = "SerialPort.Open";
            port.Open();
            LogHost("SERIAL_PORT_OPEN", DescribeSerialPort(port));

            var serialStream = new CancellableSerialPortStream(port);
            _serialStream = serialStream;
            var session = new MacStudioAgentSession(serialStream);
            _session = session;
            _connectionStage = "Agent HELLO handshake";
            var agent = await session.ConnectAsync(cancellation.Token);
            if (_isClosing)
            {
                return;
            }

            if ((agent.SupportedBaudRates &
                MacStudioAgentBaudRates.GetCapabilityFlag(baudRate)) == 0)
            {
                throw new InvalidDataException(
                    $"The Agent does not advertise support for {baudRate:N0} baud.");
            }

            _connectionStage = "send host diagnostics to Agent";
            var hostDiagnostics = CreateHostDiagnosticsPayload(port, baudRate);
            await session.SendAsync(
                new MacStudioAgentFrame(
                    (byte)MacStudioAgentMessageType.HostDiagnostics,
                    session.SessionId,
                    hostDiagnostics),
                cancellation.Token);
            LogHost(
                "HOST_DIAGNOSTICS_SENT",
                Encoding.ASCII.GetString(hostDiagnostics));

            SetStatus($"Connected to the MacStudio Agent on {portName}.");
            AgentDetailsText.Text = string.Format(
                CultureInfo.InvariantCulture,
                "Agent v{0}.{1}.{2} (protocol {3})\nSystem {4}.{5}; machine type {6}\nCapabilities: 0x{7:X8}; free bytes: {8:N0}\nSupported baud flags: 0x{9:X8}\nConnection baud rate: {10:N0}\nHost log: {11}",
                agent.AgentMajor,
                agent.AgentMinor,
                agent.AgentPatch,
                agent.ProtocolVersion,
                agent.SystemMajor,
                agent.SystemMinor,
                agent.MachineType,
                agent.Capabilities,
                agent.FreeBytes,
                agent.SupportedBaudRates,
                baudRate,
                HostDiagnosticsPath);
            StartDebugReceiver(session);
            LogHost(
                "AGENT_HANDSHAKE_SUCCESS",
                $"Agent={agent.AgentMajor}.{agent.AgentMinor}.{agent.AgentPatch}; System={agent.SystemMajor}.{agent.SystemMinor}; MachineType={agent.MachineType}; Capabilities=0x{agent.Capabilities:X8}; FreeBytes={agent.FreeBytes}; SupportedBaudFlags=0x{agent.SupportedBaudRates:X8}");
        }
        catch (OperationCanceledException)
        {
            LogHost(
                "CONNECT_CANCELLED",
                $"Stage={_connectionStage}; cancellation requested by host.");
            if (!_isClosing)
            {
                SetStatus("Connection cancelled.");
            }
            ResetConnection();
        }
        catch (Exception error) when (
            error is UnauthorizedAccessException or
                IOException or
                InvalidDataException or
                ArgumentException or
                InvalidOperationException or
                TimeoutException or
                Win32Exception)
        {
            LogHost(
                "CONNECT_FAILURE",
                $"Stage={_connectionStage}; Type={error.GetType().FullName}; HResult=0x{error.HResult:X8}; {error}");
            if (!_isClosing)
            {
                SetStatus($"Could not connect: {error.Message}");
            }
            ResetConnection();
        }
        finally
        {
            _connectionStage = "idle";
            if (ReferenceEquals(_connectionCancellation, cancellation))
            {
                _connectionCancellation = null;
            }

            _isConnecting = false;
            UpdateControls();
        }
    }

    private void RefreshPorts()
    {
        try
        {
            var selectedPort = ComPortComboBox.SelectedItem as string;
            var ports = SerialPort.GetPortNames()
                .OrderBy(name => name, StringComparer.OrdinalIgnoreCase)
                .ToArray();

            ComPortComboBox.Items.Clear();
            foreach (var port in ports)
            {
                ComPortComboBox.Items.Add(port);
            }
            _availableComPorts = ports;
            LogHost(
                "COM_PORTS_ENUMERATED",
                ports.Length == 0 ? "(none)" : string.Join(", ", ports));

            if (selectedPort is not null &&
                ports.Contains(selectedPort, StringComparer.OrdinalIgnoreCase))
            {
                ComPortComboBox.SelectedItem = ports.First(
                    port => string.Equals(
                        port,
                        selectedPort,
                        StringComparison.OrdinalIgnoreCase));
            }
            else if (ports.Length > 0)
            {
                ComPortComboBox.SelectedIndex = 0;
            }

            SetStatus(ports.Length == 0
                ? "No PC COM ports are available. Connect an adapter or create a virtual COM pair, then refresh."
                : $"{ports.Length} PC COM port(s) available.");
        }
        catch (Win32Exception error)
        {
            _availableComPorts = [];
            LogHost("COM_PORT_ENUMERATION_FAILURE", error.ToString());
            SetStatus($"Could not enumerate PC COM ports: {error.Message}");
        }
    }

    private void ResetConnection()
    {
        _session = null;
        var serialStream = _serialStream;
        _serialStream = null;
        serialStream?.Dispose();
        var port = _serialPort;
        _serialPort = null;
        port?.Dispose();
    }

    private void UpdateControls()
    {
        var connected = _session?.State == MacStudioAgentConnectionState.Connected;
        ConnectButton.IsEnabled = !_isConnecting && !connected;
        DisconnectButton.IsEnabled = connected && !_isTransferring && !_isCommandInProgress;
        SendPackageButton.IsEnabled = connected &&
            !_isTransferring &&
            !_isCommandInProgress &&
            !_isApplicationRunning;
        CancelTransferButton.IsEnabled = _isTransferring;
        RunButton.IsEnabled = connected &&
            !_isTransferring &&
            !_isCommandInProgress &&
            !_isApplicationRunning &&
            _lastBuildId is not null;
        StopButton.IsEnabled = connected &&
            !_isTransferring &&
            !_isCommandInProgress &&
            _isApplicationRunning;
        DestinationNameTextBox.IsEnabled =
            connected && !_isTransferring && !_isCommandInProgress;
        ComPortComboBox.IsEnabled = !_isConnecting && !connected;
        BaudRateComboBox.IsEnabled = !_isConnecting && !connected;
        RefreshPortsButton.IsEnabled = !_isConnecting && !connected;
    }

    private int GetSelectedBaudRate()
    {
        if (BaudRateComboBox.SelectedItem is not System.Windows.Controls.ComboBoxItem item ||
            !int.TryParse(
                item.Content?.ToString(),
                NumberStyles.None,
                CultureInfo.InvariantCulture,
                out var baudRate))
        {
            throw new InvalidOperationException("Select a supported Mac baud rate.");
        }

        _ = MacStudioAgentBaudRates.GetCapabilityFlag(baudRate);
        return baudRate;
    }

    private void SetStatus(string status)
    {
        if (!_isClosing)
        {
            StatusText.Text = status;
            LogHost("STATUS", status);
        }
    }

    private static string GetHostMachineDetails()
    {
        var applicationVersion =
            typeof(SerialAgentConnectionWindow).Assembly.GetName().Version?.ToString()
            ?? "unknown";
        return string.Join(
            "; ",
            $"Machine={Environment.MachineName}",
            $"OS={RuntimeInformation.OSDescription}",
            $"OSArchitecture={RuntimeInformation.OSArchitecture}",
            $"ProcessArchitecture={RuntimeInformation.ProcessArchitecture}",
            $"64BitOS={Environment.Is64BitOperatingSystem}",
            $"64BitProcess={Environment.Is64BitProcess}",
            $"Runtime={RuntimeInformation.FrameworkDescription}",
            $"RuntimeVersion={Environment.Version}",
            $"ClassicMacStudioVersion={applicationVersion}");
    }

    private byte[] CreateHostDiagnosticsPayload(SerialPort port, int baudRate)
    {
        var details = string.Join(
            "; ",
            $"Host={Environment.MachineName}",
            $"OS={RuntimeInformation.OSDescription}",
            $"OSArch={RuntimeInformation.OSArchitecture}",
            $"ProcessArch={RuntimeInformation.ProcessArchitecture}",
            $"Runtime={RuntimeInformation.FrameworkDescription}",
            $"IDE={typeof(SerialAgentConnectionWindow).Assembly.GetName().Version}",
            $"COM={port.PortName}",
            $"COMPorts={(_availableComPorts.Length == 0 ? "(none)" : string.Join(",", _availableComPorts))}",
            $"Open={port.IsOpen}",
            $"Baud={baudRate}",
            "Format=8N1",
            "Handshake=None",
            "Probe=selected-port-open-and-Agent-HELLO");
        details = SanitizeDiagnosticField(details);
        if (details.Length > 512)
        {
            details = details[..509] + "...";
        }

        return Encoding.ASCII.GetBytes(details);
    }

    private static string DescribeSerialPort(SerialPort port) =>
        string.Join(
            "; ",
            $"Port={port.PortName}",
            $"Open={port.IsOpen}",
            $"Baud={port.BaudRate}",
            $"Parity={port.Parity}",
            $"DataBits={port.DataBits}",
            $"StopBits={port.StopBits}",
            $"Handshake={port.Handshake}",
            $"DtrEnable={port.DtrEnable}",
            $"RtsEnable={port.RtsEnable}",
            $"ReadTimeout={port.ReadTimeout}",
            $"WriteTimeout={port.WriteTimeout}");

    private static string SanitizeDiagnosticField(string value) =>
        string.Concat(value.Select(character =>
            char.IsControl(character) || character is '\r' or '\n'
                ? ' '
                : character));

    private void LogHost(string eventName, string details)
    {
        var safeDetails = SanitizeDiagnosticField(details);
        var line = string.Format(
            CultureInfo.InvariantCulture,
            "{0:O}\t{1}\t{2}{3}",
            DateTimeOffset.Now,
            eventName,
            safeDetails,
            Environment.NewLine);
        try
        {
            lock (_diagnosticsLock)
            {
                Directory.CreateDirectory(HostDiagnosticsDirectory);
                File.AppendAllText(HostDiagnosticsPath, line, Encoding.UTF8);
            }
        }
        catch (Exception error) when (
            error is IOException or
                UnauthorizedAccessException or
                ArgumentException or
                NotSupportedException)
        {
            DebugLogTextBox.AppendText(
                $"[HOST LOG ERROR] {error}{Environment.NewLine}");
            DebugLogTextBox.ScrollToEnd();
        }
    }
}
