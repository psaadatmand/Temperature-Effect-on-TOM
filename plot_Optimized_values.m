clc;                % clear the Command Window
close all;          % close all opened figure windows
clear all;          % clear all variables in the workspace
format long g;      % set the format of numeric output ("long general")
format compact;     % set the format of output: remove extra line spacing
fontSize = 15;      % set font

% human subject test description
% ========================================================================
humanSubjectName = '';
humanTestType = '_normal'; % '_normal', '_occlusion' or '_hypoxia'
humanTestNumber = 1;

% plotting data
% ========================================================================
save_plot_data = false;                      
save_plot_data_fig = false;
save_plot_data_svg = false;
save_plot_data_png = false;

plot_decay_curves = false;
plot_tau_values = false;
plot_calibratedtau_values = false;
plot_tau_histogram = false;
plot_temperature_values = false;
plot_temp_histogram = false;
plot_temperature_ADC_values = false;
plot_acceleration_values = false;
plot_transition_points = false;
scalingfactor_plot = true;
plot_taucorrected_histogram = true;

plot_Lifetime_GN = false; 
plot_calibrated_Lifetime_GN = false;

plot_Lifetime_BFGS = false; 
plot_calibrated_Lifetime_BFGS = false;

plot_Lifetime_GD = false; 
plot_calibrated_Lifetime_GD = false;


plot_Lifetime_GDM = false; 
plot_calibrated_Lifetime_GDM = false;

plot_Lifetime_GDA = false; 
plot_calibrated_Lifetime_GDA = false;

plot_Lifetime_LM = true; 
plot_calibrated_Lifetime_LM = true;


% read out the file with settings
% ========================================================================
fileID = fopen('settings.txt', 'r');
formatSpec = '%s %q';  % string, quoted string
data = textscan(fileID, formatSpec);
fclose(fileID);

setting_names = data{1};
setting_values = data{2};

% setup model's parameters
rawData = "";
% ========================================================================

for i = 1 : numel(setting_names)
    if setting_names{i} == "plotFolder"
        plotFolder = setting_values{i};
    end

    if setting_names{i} == "rawData"
        rawData = setting_values{i};
    end

    if setting_names{i} == "darkData"
        darkData = setting_values{i};
    end  

    if setting_names{i} == "samplesPerDecayCurve"
        % keep str2num (not str2double) since we work on integers here
        samplesPerDecayCurve = str2num(setting_values{i});   
    end 

    if setting_names{i} == "numberOfMeasurements"
        numberOfMeasurements = str2num(setting_values{i});   
    end 

    if setting_names{i} == "samplingStartOffsetUsec"
        samplingStartOffsetUsec = str2num(setting_values{i});
        if mod(samplingStartOffsetUsec,2) == 1
            error(['Sampling start offset must be an even number ' ...
                'due to aligment of dark data.'])
        end
    end

    if setting_names{i} == "rawDataParseStartIndex"
        rawDataParseStartIndex = str2num(setting_values{i});   
    end 

    if setting_names{i} == "measurementTime_min"
        measurementTime_min = str2num(setting_values{i});   
    end

    if setting_names{i} == "eventChangePoints"
        eventChangePoints = str2num(setting_values{i});   
    end 

    if setting_names{i} == "dataDecimationFactor"
        dataDecimationFactor = str2num(setting_values{i});   
    end 
end

% user-defined variables
% ========================================================================
% time between adjacent decay curves
measurementPeriod_ms = floor(measurementTime_min * 60 * 1000 / ...
                             numberOfMeasurements);

% array to store calculated tau values
tauValues = zeros(1, numberOfMeasurements - rawDataParseStartIndex + 1);
% array to store calibrated tau values
calibratedtau = zeros(1, numberOfMeasurements - rawDataParseStartIndex + 1);
% array to store time point at which tau values are calculated
times = zeros(1,numberOfMeasurements - rawDataParseStartIndex + 1);

% the start index to process the tau measurement results
process_tau_start_index = 9; 

% read raw fluorescence data
%  =======================================================================
fluorescenceData = readlines(rawData);
fluorescenceData(cellfun('isempty', fluorescenceData)) = [];
fluorescenceDataArray = get_fluorescence_data(fluorescenceData, ...
                                                samplesPerDecayCurve, ...
                                                numberOfMeasurements);
fluorescenceDataArray = fluorescenceDataArray(:, rawDataParseStartIndex:...
                                              numberOfMeasurements);

% read data
%  =======================================================================

lifetime_GN_Data = get_lifetime_GN(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);

calibrated_lifetime_GN_Data = get_calibrated_lifetime_GN(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);

% lifetime_BFGS_Data = get_lifetime_BFGS(fluorescenceData, ...
%                                         numberOfMeasurements ...
%                                         - rawDataParseStartIndex + 1);
% 
% calibrated_lifetime_BFGS_Data = calibrated_lifetime_BFGS_Data(fluorescenceData, ...
%                                         numberOfMeasurements ...
%                                         - rawDataParseStartIndex + 1);

lifetime_GD_Data = get_lifetime_GD(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);

calibrated_lifetime_GD_Data = get_calibrated_lifetime_GD(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);

lifetime_GDM_Data = get_lifetime_GDM(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);

calibrated_lifetime_GDM_Data = get_calibrated_lifetime_GDM(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);
lifetime_GDA_Data = get_lifetime_GDA(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);

calibrated_lifetime_GDA_Data = get_calibrated_lifetime_GDA(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);

lifetime_LM_Data = get_lifetime_LM(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);

calibrated_lifetime_LM_Data = get_calibrated_lifetime_LM(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);

scaling_factor = get_scaling_factor(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);



accelerationData = get_accelerationData(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);
temperatureData = get_temperatureData(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);
temperature_ADCData = get_temperature_ADCData(fluorescenceData, ...
                                        numberOfMeasurements ...
                                        - rawDataParseStartIndex + 1);

% read dark data to compensate for the offset
%  =======================================================================
darkDataToSubtracted = table2array(readtable(darkData));
% number of saturated dark data points
darkDataPointsInSaturationRegion = 0;
darkDataPointsToSkip = darkDataPointsInSaturationRegion ...
                           + samplingStartOffsetUsec / 2;
darkDataToSubtracted = darkDataToSubtracted(darkDataPointsToSkip + 1 ...
                           : darkDataPointsToSkip+samplesPerDecayCurve);

% initialize the live plot
%  =======================================================================
% f2 = figure(2);
% g = animatedline('Color','r','LineStyle','none','Marker','o', ...
%                 'MarkerSize',6,'MarkerFaceColor','r', ...
%                 'MarkerEdgeColor','r');
% ylabel("Decay Time (\mus)",'FontSize',14);
% xlabel('Time (Seconds)','FontSize',14);

% set the power supply
%  =======================================================================
% myInstrument = visadev('USB0::0x2A8D::0x0602::MY57056397::INSTR');
% set_power_supply(myInstrument, 3, 0.25);

% repeat tau measurements
%  =======================================================================
% i = 1;
% while i <= (numberOfMeasurements - rawDataParseStartIndex + 1)
%     % open and close the power supply to extract curve from ADPD
% %     open_power_supply(myInstrument);
% %     pause(11);
% %     close_power_supply(myInstrument);
% 
%     currentFluorescenceData = ... 
%                        fluorescenceDataArray(:, i) - darkDataToSubtracted;
%     fluorescenceDataNecessary = ...
%                        currentFluorescenceData(process_tau_start_index: ...
%                                                samplesPerDecayCurve);
% 
%     % make exponential fit and calculate tau
%     X = 0e-6:2e-6:(size(fluorescenceDataNecessary,1)-1)*2e-6;
%     Y = fluorescenceDataNecessary;
%     Y = Y';
%     tbl = table(X', Y');
%     modelfun = @(b,x) b(1) * exp(-b(2)*x(:, 1)) + b(3);
%     beta0 = [max(Y)/1, max(X)/10, 6];
%     mdl = fitnlm(tbl, modelfun, beta0);
%     coefficients = mdl.Coefficients{:, 'Estimate'};
%     tau = 1/coefficients(2) * 1e6;
%     tauValues(i) = tau;
%     times(i) = measurementPeriod_ms*(i-1); % in seconds
% 
%     % plot the ADC data and fitted decay curve on the same plot
%     if plot_decay_curves == true
%         figure;
%         plot(X*1e6, Y, 'b.', 'LineWidth', 2, 'MarkerSize', 15);
%         grid on;
%         yFitted = coefficients(1) * exp(-coefficients(2)*X) + ...
%                   coefficients(3);
%         hold on;
%         plot(X*1e6, yFitted, 'r-', 'LineWidth', 2);
%         grid on;
%         title({['\tau = ', num2str(tau), ' \mus'] [' g_x = ' , ...
%               num2str(accelerationData(i,1)), ' g, g_y = ' , ...
%               num2str(accelerationData(i,2)), ' g, g_z = ' , ...
%               num2str(accelerationData(i,3)), ' g']}, 'FontSize', ...
%                       fontSize);
%         xlabel('Time (\mus)', 'FontSize', fontSize);
%         ylabel('Fluorescence (ADC Code)', 'FontSize', fontSize);
%         legendHandle = legend('Data', 'Fitted', 'Location', 'north');
%         legendHandle.FontSize = 30;
% 
%         % save plotted data
%         if save_plot_data == true
%             % save fig (MATLAB)
%             if save_plot_data_fig
%                 plotFileName = fullfile(plotFolder, ['plot_', ...
%                                                      num2str(i), '.fig']);
%                 saveas(gcf, plotFileName, 'fig');
%             end
%             % save png
%             if save_plot_data_png == true
%                 plotFileName = fullfile(plotFolder, ['plot_', ...
%                                                      num2str(i), '.png']);
%                 saveas(gcf, plotFileName, 'png');
%             end
%             % save svg
%             if save_plot_data_svg == true
%                 plotFileName = fullfile(plotFolder, ['plot_', ...
%                                                      num2str(i), '.svg']);
%                 saveas(gcf, plotFileName, 'svg');
%             end
%         end
%     end
% 
% % plot tau histogram
% 
% if plot_tau_histogram == true
%      if i > 3
%         avrg = mean(tauValues(2:i-1));
%         sigma = std(tauValues(2:i-1));
%         histfit(tauValues(2:i-1),20);
%         set(gca,'FontSize',15);
%         xlabel("\tau [\mus]",'FontSize',15);
%         ylabel('Count','FontSize',15);
%         title(['\mu = ', num2str(avrg), ' \mus, \sigma = ', ...
%                                          num2str(sigma), ' \mus']);
%         xlim([avrg - 3 * sigma avrg + 3 * sigma]);
%         drawnow
%      end
% end 
% 
% 
%     % display the current time from the beginning of experiment and the
%     % calculated tau value and write them into an excel file
% %     sprintf("Tau is %.5f us and Time is %d seconds",tau,13*(i-1))
% %     index1 = ['A',num2str(i)];
% %     index2 = ['B',num2str(i)];
% %     name = ['human_subject_',humanTestType,humanSubjectName, ...
% %            datetime("today"),num2str(humanTestNumber),'.xlsx'];
% %     xlswrite(name,measurementPeriod_ms*(i-1),1,index1);
% %     xlswrite(name,tau,1,index2);
% 
%     % update the live plot
% %     f2;
% %     addpoints(g,measurementPeriod*(i-1),tauValues(i));
% %     drawnow
% 
%     % increment counter
%     i = i + 1;
% end
% 
% % calibration algorithm
% % ========================================================================
% 
% temp_min = 22:0.1:42;
% temp_max = temp_min + 0.1;
% scales = linspace(0.98, 1.22, length(temp_min));
% 
% for i = 1:length(temperatureData)
%     temp = temperatureData(i);
%     % Find corresponding scaling factor
%     for j = 1:length(temp_min)
%         if temp >= temp_min(j) && temp < temp_max(j)
%             calibratedtau(i) = lifetime_LM_Data(i) * scales(j);
%             break;
%         end
%     end
% end
% 
% % plot tau 
% % ========================================================================
% 
% if plot_tau_values == true
%     times_tau = decimate(times, dataDecimationFactor);
%     tauValues_plot = decimate(tauValues, dataDecimationFactor); 
%     figure;
%     plot(times_tau./1000, tauValues_plot, 'b.', 'LineWidth', 2, 'MarkerSize', 15);
%     xlabel('Time [s]', 'FontSize', fontSize);
%     ylabel('\tau [\mus]', 'FontSize', fontSize);
%     title('Tau vs Time', 'FontSize', fontSize);
% 
% 
%     if plot_calibratedtau_values == true
%         times_tau = decimate(times, dataDecimationFactor);
%         calibratedtauValues_plot = decimate(calibratedtau, dataDecimationFactor); 
%         figure;
%         plot(times_tau./1000, calibratedtauValues_plot, 'b.', 'LineWidth', 2, 'MarkerSize', 15);
%         xlabel('Time [s]', 'FontSize', fontSize);
%         ylabel('calibrated \tau [\mus]', 'FontSize', fontSize);
%         title('Calibrated \tau vs Time', 'FontSize', fontSize);
%     end
% 
%     % put vertical lines to indicate time point at which event changed
%     if plot_transition_points == true
%         for j=1:size(eventChangePoints,2)
%             xline(eventChangePoints(1,j)*60,'--','LineWidth',2);
%         end
%     end
% 
%     % save plotted data
%     if save_plot_data == true
%         % save fig (MATLAB)
%         if save_plot_data_fig
%             plotFileName = fullfile(plotFolder, ['tau_values', '.fig']);
%             saveas(gcf, plotFileName, 'fig');
%         end
%         % save png
%         if save_plot_data_png == true
%             plotFileName = fullfile(plotFolder, ['tau_values', '.png']);
%             saveas(gcf, plotFileName, 'png');
%         end
%         % save svg
%         if save_plot_data_svg == true
%             plotFileName = fullfile(plotFolder, ['tau_values', '.svg']);
%             saveas(gcf, plotFileName, 'svg');
%         end
%     end
% end
% % ==========================================
% plot temperature Data
% ========================================================================
% Check if plotting is enabled
if plot_temperature_values
    % Check if necessary variables are defined
    if exist('times', 'var') && exist('temperatureData', 'var') && exist('dataDecimationFactor', 'var')
        % Decimate the time and temperature data
        times_temp = decimate(times, dataDecimationFactor);
        temperatureData_plot = decimate(temperatureData, dataDecimationFactor);

        % Plot the decimated data
        
        figure;
        plot(times_temp ./ 1000, temperatureData_plot, 'r.', 'LineWidth', 2, 'MarkerSize', 15);
        xlabel('Time [s]', 'FontSize', fontSize);
        ylabel('Temperature [C]', 'FontSize', fontSize);
        title('Temperature vs Time', 'FontSize', fontSize);
       
    else
        % Handle case where necessary variables are missing
        error('Missing variables for plotting.');
    end
else
    disp('Plotting disabled.');
end

% plot temperature histogram
% =======================================================================

if plot_temp_histogram == true
     if i > 3
        avrg = mean(temperatureData(2:i-1));
        sigma = std(temperatureData(2:i-1));
        histfit(temperatureData(2:i-1),20);
        set(gca,'FontSize',15);
        xlabel("\temp [C]",'FontSize',15);
        ylabel('Count','FontSize',15);
        title(['Temp = ', num2str(avrg), ' C, \sigma = ', ...
                                         num2str(sigma), ' C']);
        xlim([avrg - 3 * sigma avrg + 3 * sigma]);
        drawnow
     end
end 
% plot Tau with respect to temperature Data
% =======================================================================


%  figure;
%  plot(temperatureData, tauValues, 'r.', 'LineWidth',2, 'MarkerSize', 15, DisplayName='\tau');
%  hold on;
%  plot(temperatureData, calibratedtauValues_plot, 'b.', 'LineWidth',2, 'MarkerSize', 15, DisplayName='calibrated \tau');
%  xlabel('temperature [C]', 'FontSize', fontSize);
%  ylabel('calibrated \tau value [us]', 'FontSize', fontSize);
%  title('calibrated \tau vs Temperature', 'FontSize', fontSize);
%  legend show;
%  hold off;
% % plot temperature Data
% ========================================================================


if scalingfactor_plot
    % Check if necessary variables are defined
    if exist('temperatureData', 'var') && exist('scaling_factor', 'var') && exist('dataDecimationFactor', 'var')
        % Decimate the time and lifetime_GN data
        scaling_factor_plot = decimate(scaling_factor, dataDecimationFactor);

        % Plot the decimated data
        
        figure;
        grid on;
        plot(temperatureData, scaling_factor_plot, 'r.', 'LineWidth', 2, 'MarkerSize', 15, 'DisplayName','\tau');
        xlabel('Temperature [C]', 'FontSize', fontSize);
        ylabel('scaling factor [us]', 'FontSize', fontSize);
        %title('Lifetime vs Temperature', 'FontSize', fontSize);
        hold on;
       
    else
        % Handle case where necessary variables are missing
        error('Missing variables for plotting.');
    end
else
    disp('Plotting disabled.');
end


% Check if plotting is enabled
if plot_Lifetime_GN
    % Check if necessary variables are defined
    if exist('times', 'var') && exist('lifetime_GN_Data', 'var') && exist('dataDecimationFactor', 'var')
        % Decimate the time and lifetime_GN data
        lifetime_GN_Data_plot = decimate(lifetime_GN_Data, dataDecimationFactor);

        % Plot the decimated data
        
        figure;
        grid on;
        plot(temperatureData, lifetime_GN_Data_plot, 'r.', 'LineWidth', 2, 'MarkerSize', 15);
        xlabel('Temperature [C]', 'FontSize', fontSize);
        ylabel('Lifetime_GN [us]', 'FontSize', fontSize);
        title('Lifetime vs Temperature', 'FontSize', fontSize);
       
    else
        % Handle case where necessary variables are missing
        error('Missing variables for plotting.');
    end
else
    disp('Plotting disabled.');
end


if plot_calibrated_Lifetime_GN
    % Check if necessary variables are defined
    if exist('times', 'var') && exist('calibrated_lifetime_GN_Data', 'var') && exist('dataDecimationFactor', 'var')
        % Decimate the time and lifetime_GN data
        Calibrated_Lifetime_GN_Data_plot = decimate(calibrated_lifetime_GN_Data, dataDecimationFactor);

        % Plot the decimated data
        
        figure;
        grid on;
        plot(temperatureData, Calibrated_Lifetime_GN_Data_plot, 'r.', 'LineWidth', 2, 'MarkerSize', 15);
        xlabel('Temperature [C]', 'FontSize', fontSize);
        ylabel('Calibrated_Lifetime_GN [us]', 'FontSize', fontSize);
        title('Calibrated_Lifetime_GN vs Temperature', 'FontSize', fontSize);

    else
        % Handle case where necessary variables are missing
        error('Missing variables for plotting.');
    end
else
    disp('Plotting disabled.');
end


% ========================================================================
% Check if plotting is enabled
if plot_Lifetime_BFGS
    % Check if necessary variables are defined
    if exist('times', 'var') && exist('lifetime_BFGS_Data', 'var') && exist('dataDecimationFactor', 'var')
        % Decimate the time and lifetime_GN data
        lifetime_BFGS_Data_plot = decimate(lifetime_BFGS_Data, dataDecimationFactor);

        % Plot the decimated data
        
        figure;
        grid on;
        plot(temperatureData, lifetime_BFGS_Data_plot, 'r.', 'LineWidth', 2, 'MarkerSize', 15);
        xlabel('Temperature [C]', 'FontSize', fontSize);
        ylabel('Lifetime_BFGS [us]', 'FontSize', fontSize);
        title('Lifetime vs Temperature', 'FontSize', fontSize);
       
    else
        % Handle case where necessary variables are missing
        error('Missing variables for plotting.');
    end
else
    disp('Plotting disabled.');
end


if plot_calibrated_Lifetime_BFGS
    % Check if necessary variables are defined
    if exist('times', 'var') && exist('calibrated_lifetime_BFGS_Data', 'var') && exist('dataDecimationFactor', 'var')
        % Decimate the time and lifetime_GN data
        Calibrated_Lifetime_BFGS_Data_plot = decimate(calibrated_lifetime_BFGS_Data, dataDecimationFactor);

        % Plot the decimated data
        
        figure;
        grid on;
        plot(temperatureData, Calibrated_Lifetime_BFGS_Data_plot, 'r.', 'LineWidth', 2, 'MarkerSize', 15);
        xlabel('Temperature [C]', 'FontSize', fontSize);
        ylabel('Calibrated_Lifetime_BFGS [us]', 'FontSize', fontSize);
        title('Calibrated_Lifetime_BFGS vs Temperature', 'FontSize', fontSize);

    else
        % Handle case where necessary variables are missing
        error('Missing variables for plotting.');
    end
else
    disp('Plotting disabled.');
end


% ========================================================================
% Check if plotting is enabled
if plot_Lifetime_GDM
    % Check if necessary variables are defined
    if exist('times', 'var') && exist('lifetime_GDM_Data', 'var') && exist('dataDecimationFactor', 'var')
        % Decimate the time and lifetime_GN data
        lifetime_GDM_Data_plot = decimate(lifetime_GDM_Data, dataDecimationFactor);

        % Plot the decimated data
        
        figure;
        grid on;
        plot(temperatureData, lifetime_GDM_Data_plot, 'r.', 'LineWidth', 2, 'MarkerSize', 15);
        xlabel('Temperature [C]', 'FontSize', fontSize);
        ylabel('Lifetime GDM [us]', 'FontSize', fontSize);
        title('Lifetime vs Temperature', 'FontSize', fontSize);
       
    else
        % Handle case where necessary variables are missing
        error('Missing variables for plotting.');
    end
else
    disp('Plotting disabled.');
end


if plot_calibrated_Lifetime_GDM
    % Check if necessary variables are defined
    if exist('times', 'var') && exist('calibrated_lifetime_GDM_Data', 'var') && exist('dataDecimationFactor', 'var')
        % Decimate the time and lifetime_GN data
        Calibrated_Lifetime_GDM_Data_plot = decimate(calibrated_lifetime_GDM_Data, dataDecimationFactor);

        % Plot the decimated data
        
        figure;
        grid on;
        plot(temperatureData, Calibrated_Lifetime_GDM_Data_plot, 'r.', 'LineWidth', 2, 'MarkerSize', 15);
        xlabel('Temperature [C]', 'FontSize', fontSize);
        ylabel('Calibrated Lifetime GDM [us]', 'FontSize', fontSize);
        title('Calibrated Lifetime GDM vs Temperature', 'FontSize', fontSize);

    else
        % Handle case where necessary variables are missing
        error('Missing variables for plotting.');
    end
else
    disp('Plotting disabled.');
end



% ========================================================================
% Check if plotting is enabled
plot_Lifetime_GDA = true;
if plot_Lifetime_GDA
    % Check if necessary variables are defined
    if exist('times', 'var') && exist('lifetime_GDA_Data', 'var') && exist('dataDecimationFactor', 'var')
        % Decimate the time and lifetime_GN data
        lifetime_GDA_Data_plot = decimate(lifetime_GDA_Data, dataDecimationFactor);

        % Plot the decimated data
        
        figure;
        grid on;
        plot(temperatureData, lifetime_GDA_Data_plot, 'r.', 'LineWidth', 2, 'MarkerSize', 15);
        xlabel('Temperature [C]', 'FontSize', fontSize);
        ylabel('Lifetime GDA [us]', 'FontSize', fontSize);
        title('Lifetime vs Temperature', 'FontSize', fontSize);
        hold on;
       
    else
        % Handle case where necessary variables are missing
        error('Missing variables for plotting.');
    end
else
    disp('Plotting disabled.');
end

plot_calibrated_Lifetime_GDA = true;
if plot_calibrated_Lifetime_GDA
    % Check if necessary variables are defined
    if exist('times', 'var') && exist('calibrated_lifetime_GDA_Data', 'var') && exist('dataDecimationFactor', 'var')
        % Decimate the time and lifetime_GN data
        Calibrated_Lifetime_GDA_Data_plot = decimate(calibrated_lifetime_GDA_Data, dataDecimationFactor);

        % Plot the decimated data
        
        
        grid on;
        plot(temperatureData, Calibrated_Lifetime_GDA_Data_plot, 'g.', 'LineWidth', 2, 'MarkerSize', 15);
        xlabel('Temperature [C]', 'FontSize', fontSize);
        ylabel('Calibrated Lifetime GDA [us]', 'FontSize', fontSize);
        title('Calibrated Lifetime GDA vs Temperature', 'FontSize', fontSize);
        hold off;

    else
        % Handle case where necessary variables are missing
        error('Missing variables for plotting.');
    end
else
    disp('Plotting disabled.');
end

plot_lifetime_histogram = true;

% if plot_lifetime_histogram == true
%      if i > 3
%         avrg = mean(lifetime_LM_Data(2:i-1));
%         sigma = std(lifetime_LM_Data(2:i-1));
%         histfit(lifetime_LM_Data(2:i-1),20);
%         set(gca,'FontSize',15);
%         xlabel("\tau [\mus]",'FontSize',15);
%         ylabel('Count','FontSize',15);
%         title(['\tau = ', num2str(avrg), ' \mus, \sigma = ', ...
%                                          num2str(sigma), ' \mus']);
%         xlim([avrg - 3 * sigma avrg + 3 * sigma]);
%         drawnow
%      end
% end 

plot_Lifetime_LM = true;
% ========================================================================
% Check if plotting is enabled
if plot_Lifetime_LM == true
    % Check if necessary variables are define
        % Decimate the time and lifetime_GN data
        lifetime_LM_Data_plot = decimate(lifetime_LM_Data, dataDecimationFactor);

        % Plot the decimated data
        
        figure;
        grid on;
        plot(temperatureData, lifetime_LM_Data, 'r.', 'LineWidth', 2, 'MarkerSize', 15, 'DisplayName','\tau');
        xlabel('Temperature [C]', 'FontSize', fontSize);
        ylabel('Lifetime LM [us]', 'FontSize', fontSize);
        title('Lifetime vs Temperature', 'FontSize', fontSize);
        hold off;
       
    %else
        % Handle case where necessary variables are missing
        error('Missing variables for plotting.');
    %end
else
    disp('Plotting disabled.');
end

plot_calibrated_Lifetime_LM = true;
if plot_calibrated_Lifetime_LM == true
    % Check if necessary variables are defined
        % Decimate the time and lifetime_GN data
        CalibratedLifetime_LM_Data_plot = decimate(calibrated_lifetime_LM_Data, dataDecimationFactor);
        x=1
        % Plot the decimated data
        figure;
        grid on;
        plot(temperatureData, CalibratedLifetime_LM_Data_plot, 'g.', 'LineWidth', 2, 'MarkerSize', 15,'DisplayName','Calibrated \tau');
        xlabel('Temperature [C]', 'FontSize', fontSize);
        ylabel('\tau[\mu s]', 'FontSize', fontSize);
        title('Calibrated Lifetime LM vs Temperature', 'FontSize', fontSize);
        legend show;
        hold off;

else
    disp('Plotting disabled.');
end

if plot_taucorrected_histogram == true
     if i > 3
        avrg = mean(Calibrated_Lifetime_GDA_Data_plot(2:i-1));
        sigma = std(Calibrated_Lifetime_GDA_Data_plot(2:i-1));
        histfit(Calibrated_Lifetime_GDA_Data_plot(2:i-1),20);
        set(gca,'FontSize',15);
        xlabel("\tau [\mus]",'FontSize',15);
        ylabel('Count','FontSize',15);
        title(['\mu = ', num2str(avrg), ' \mus, \sigma = ', ...
               num2str(sigma), ' \mus']);
        xlim([avrg - 3 * sigma avrg + 3 * sigma]);
        drawnow
     end
end 
% plot temperature histogram
% =======================================================================

% plot Tau with respect to temperature Data
% =======================================================================


 % figure;
 % plot(temperatureData, tauValues, 'r.', 'LineWidth',2, 'MarkerSize', 15, 'DisplayName','\tau');
 % hold on;
 % plot(temperatureData, calibratedtauValues_plot, 'g.', 'LineWidth',2, 'MarkerSize', 15, 'DisplayName', 'calibrated \tau');
 % xlabel('temperature [C]', 'FontSize', fontSize);
 % ylabel('calibrated \tau value [\mu s]', 'FontSize', fontSize);
 % %title('calibrated \tau vs Temperature', 'FontSize', fontSize);
 % legend show;
 % grid on;
 % hold off;

%  figure;
% scatter(times_temp ./ 1000, tauValues, 50, temperatureData, 'filled'); % Colored scatter plot
% hold on;
% scatter(times_temp ./ 1000, calibratedtauValues_plot, 50, temperatureData, 'filled'); % Colored scatter plot
% hold off;
% colormap(jet); % Set colormap from blue to red
% colorbar; % Show color scale
% caxis([min(temperature) max(temperature)]); % Set color axis range
% 
% % Labels & Formatting
% xlabel('Time (Hours)');
% ylabel('\tau (\mus)');
% title('\tau vs Time with Temperature Color Scale');
% grid on;
% xtickformat('HH:mm'); % Format x-axis time


% plot temperature Data
% ========================================================================
% plot moving averaging for temperature values
% plot moving averaging for Tau values

% Choose a window size for the moving average filter
window_size = 5;

% % Compute the moving average using the 'conv' function
% tau_moving_avg = conv(tauValues, ones(1, window_size) / window_size, 'valid');
% calibratedtau_moving_avg = conv(calibratedtauValues_plot, ones(1, window_size) / window_size, 'valid');
% 
% % Display the moving average values
% % Choose a window size for the moving average filter
% 
% % Compute the moving average using the 'conv' function
% temperature_moving_avg = conv(temperatureData, ones(1, window_size) / window_size, 'valid');


% Plot the original signal and the moving average
% figure;
% plot(temperature_moving_avg, tau_moving_avg, 'r.', 'LineWidth',2, 'MarkerSize', 15);
% hold on;
% plot(temperature_moving_avg,calibratedtau_moving_avg, 'g.', 'LineWidth',2, 'MarkerSize', 15);
% title('Tau Moving Average');
% hold off;
% grid on;



% Plot the original signal and the moving average
% figure;
% plot(scales, 'r.', 'LineWidth',2, 'MarkerSize', 15);
% title('Scaling factor at different temperature');


% ========================================================================

% % Define the folder and file name
% folder = 'E:\Gas_experiments\Temperature\Increasing_Temperature\6%';  % Change this to the path where you want to save the file
% fileName = 'Increase_temp_O2=6%';
% fullFileName = fullfile(folder, fileName);
% 
% % Create a table with transposed data
% data = table(tauValues', temperatureData', 'VariableNames', {'Tau_value', 'Temperature'});
% 
% % Write the table to an Excel file
% writetable(data, fullFileName);
% 
% % Display a message to indicate that the file has been saved
% fprintf('Data has been written to %s\n', fullFileName);

% =======================================================================

%Check if plotting is enabled
% if plot_temperature_ADC_values
%     % Check if necessary variables are defined
%     if exist('times', 'var') && exist('temperature_ADCData', 'var') && exist('dataDecimationFactor', 'var')
%         % Decimate the time and temperature data
%         times_tempADC = decimate(times, dataDecimationFactor);
%         temperature_ADCData_plot = decimate(temperature_ADCData, dataDecimationFactor);
% 
%         % Plot the decimated data
% 
%         figure;
%         plot(times_tempADC ./ 1000, temperature_ADCData_plot, 'r.', 'LineWidth', 2, 'MarkerSize', 15);
%         xlabel('Time [s]', 'FontSize', fontSize);
%         ylabel('Temperature_ADC', 'FontSize', fontSize);
%         title('Temperature vs Time', 'FontSize', fontSize);
% 
%     else
%         % Handle case where necessary variables are missing
%         error('Missing variables for plotting.');
%     end
% else
%     disp('Plotting disabled.');
% end
% plot acceleration values
% ========================================================================
if plot_acceleration_values == true
    % g_x
    times_gx = decimate(times, dataDecimationFactor);
    accelerationData_gx_plot = decimate(accelerationData(:, 1), ...
                                      dataDecimationFactor);
    figure;
    plot(times_gx./1000, accelerationData_gx_plot, 'r.', 'LineWidth', 2, ...
        'MarkerSize', 15);
    xlabel('Time [s]', 'FontSize', fontSize);
    ylabel('g_x [m/s^2]', 'FontSize', fontSize);

    % put vertical lines to indicate time point at which event changed
    if plot_transition_points == true
        for j=1:size(eventChangePoints,2)
            xline(eventChangePoints(1,j)*60,'--','LineWidth',2);
        end
    end

    % save plotted data
    if save_plot_data == true
        % save fig (MATLAB)
        if save_plot_data_fig
            plotFileName = fullfile(plotFolder, ['gx_values', '.fig']);
            saveas(gcf, plotFileName, 'fig');
        end
        % save png
        if save_plot_data_png == true
            plotFileName = fullfile(plotFolder, ['gx_values', '.png']);
            saveas(gcf, plotFileName, 'png');
        end
        % save svg
        if save_plot_data_svg == true
            plotFileName = fullfile(plotFolder, ['gx_values', '.svg']);
            saveas(gcf, plotFileName, 'svg');
        end
    end
    % g_y
    times_gy = decimate(times, dataDecimationFactor);
    accelerationData_gy_plot = decimate(accelerationData(:, 2), ...
                                  dataDecimationFactor);
    figure;
    plot(times_gy./1000, accelerationData_gy_plot, 'g.', 'LineWidth', 2, ...
        'MarkerSize', 15);
    xlabel('Time [s]', 'FontSize', fontSize);
    ylabel('g_y [m/s^2]', 'FontSize', fontSize);

    % put vertical lines to indicate time point at which event changed
    if plot_transition_points == true
        for j=1:size(eventChangePoints,2)
            xline(eventChangePoints(1,j)*60,'--','LineWidth',2);
        end
    end

    % save plotted data
    if save_plot_data == true
        % save fig (MATLAB)
        if save_plot_data_fig
            plotFileName = fullfile(plotFolder, ['gy_values', '.fig']);
            saveas(gcf, plotFileName, 'fig');
        end
        % save png
        if save_plot_data_png == true
            plotFileName = fullfile(plotFolder, ['gy_values', '.png']);
            saveas(gcf, plotFileName, 'png');
        end
        % save svg
        if save_plot_data_svg == true
            plotFileName = fullfile(plotFolder, ['gy_values', '.svg']);
            saveas(gcf, plotFileName, 'svg');
        end
    end

    % g_z
    times_gz = decimate(times, dataDecimationFactor);
    accelerationData_gz_plot = decimate(accelerationData(:, 3), ...
                                  dataDecimationFactor);
    figure;
    plot(times_gz./1000, accelerationData_gz_plot, 'm.', 'LineWidth', 2, ...
        'MarkerSize', 15);
    xlabel('Time [s]', 'FontSize', fontSize);
    ylabel('g_z [m/s^2]', 'FontSize', fontSize);

    % put vertical lines to indicate time point at which event changed
    if plot_transition_points == true
        for j=1:size(eventChangePoints,2)
            xline(eventChangePoints(1,j)*60,'--','LineWidth',2);
        end
    end

    % save plotted data
    if save_plot_data == true
        % save fig (MATLAB)
        if save_plot_data_fig
            plotFileName = fullfile(plotFolder, ['gz_values', '.fig']);
            saveas(gcf, plotFileName, 'fig');
        end
        % save png
        if save_plot_data_png == true
            plotFileName = fullfile(plotFolder, ['gz_values', '.png']);
            saveas(gcf, plotFileName, 'png');
        end
        % save svg
        if save_plot_data_svg == true
            plotFileName = fullfile(plotFolder, ['gz_values', '.svg']);
            saveas(gcf, plotFileName, 'svg');
        end
    end
end

% set voltage and current of power supply
%  =======================================================================
function [] = set_power_supply(myinst,voltage,current)
    myinst.write("*IDN?");
    myinst.write(':INSTrument:NSELect 1');
    vstr = [':SOURce:VOLTage:LEVel:IMMediate:AMPLitude ', ...
           num2str(voltage)];
    myinst.write(vstr);
    cstr = [':SOURce:CURRent:LEVel:IMMediate:AMPLitude ', ... 
           num2str(current)];
    myinst.write(cstr);
end

% read the current value of power supply
%  =======================================================================
function [current] = read_power_supply(myinst)
%    myinst.write('MEASure:VOLT?');
%    voltage = read(myinst,13,'string');
     myinst.write('MEASure:CURR?');
     current = read(myinst,13,'string');
%    voltage = str2double(voltage);
     current = str2double(current);
end

% open power supply
%  =======================================================================
function [] = open_power_supply(myinst)
    myinst.write(':OUTPut:STATe 1');
end

% close power supply
%  =======================================================================
function [] = close_power_supply(myinst)
    myinst.write(':OUTPut:STATe 0');
end

% read temperature data from Putty file
%  =======================================================================
function temperatureData = get_temperatureData(data, numberOfMeasurements)
    temperatureData = zeros(1, numberOfMeasurements);
    temperatureDataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Temperature:')
            % Convert the third element to double and store it in temperatureData
            temperatureData(1,temperatureDataIndex) = ...
                str2double(temp_data{1}{2});
            temperatureDataIndex = temperatureDataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    temperatureData = temperatureData(1:numberOfMeasurements);
end

% read temperature ADC data from Putty file
%  =======================================================================
function temperature_ADCData = get_temperature_ADCData(data, numberOfMeasurements)
    temperature_ADCData = zeros(1, numberOfMeasurements);
    temperature_ADCDataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        tempADC_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(tempADC_data{1}{1}, 'Temperature_ADC_value')
            % Convert the third element to double and store it in temperatureData
            temperature_ADCData(1,temperature_ADCDataIndex) = ...
                str2double(tempADC_data{1}{3});
            temperature_ADCDataIndex = temperature_ADCDataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    temperature_ADCData = temperature_ADCData(1:numberOfMeasurements);
end

% read acceleration data from Putty file
%  =======================================================================
function accelerationData = get_accelerationData(data, ...
                                                 numberOfMeasurements)
     accelerationData = zeros(numberOfMeasurements, 3);
     accelerationDataIndex = 1;

     for i = 1 : size(data,1)
        temp = textscan(data{i},'%s','Delimiter',' ')';

        if strcmp(temp{1}{1}, 'g_x') || strcmp(temp{1}{1}, 'g_y') || ...
                                                 strcmp(temp{1}{1}, 'g_z')
            acc_data = textscan(data{i},'%s','Delimiter',' ')';

            if strcmp(temp{1}{1}, 'g_x')
                  accelerationData(accelerationDataIndex, 1) = ...
                                          str2double(acc_data{1}{3});
            elseif strcmp(temp{1}{1}, 'g_y')
                  accelerationData(accelerationDataIndex, 2) = ... 
                                          str2double(acc_data{1}{3});
            else
                  accelerationData(accelerationDataIndex, 3) = ... 
                                          str2double(acc_data{1}{3});
                  accelerationDataIndex = accelerationDataIndex + 1;
            end
        end
     end

     accelerationData = accelerationData(end-numberOfMeasurements+1:end,:);
end

% read ADC data from the Putty file
%  =======================================================================
function fluorescenceData = get_fluorescence_data(data, ... 
                            samples_per_decay_curve, numberOfMeasurements)
     fluorescenceData = zeros(samples_per_decay_curve, ... 
                                numberOfMeasurements);
     fluorescenceDataIndex = 1;

     for i = 1 : size(data, 1)

        % read the i-th row and split by the whitespace
        temp = textscan(data{i}, '%s', 'Delimiter', ' ')';
        % compare the part before the whitespace to a text
        if strcmp(temp{1}{1}, 'Fluorescense[ADC')
            % for all 'samplesPerDecayCurve' rows and
            % 'fluorescenceDataIndex' column, conver values from string to
            % double and store to 'fluorescenceData'
            fluorescenceData(:, fluorescenceDataIndex) = ... 
                          str2double(data(i+1:i+samples_per_decay_curve));
            fluorescenceDataIndex = fluorescenceDataIndex + 1;
        end
     end

     fluorescenceData = ... 
                     fluorescenceData(:, end-numberOfMeasurements+1:end);
end

% read temperature data from Putty file

function scaling_factor = get_scaling_factor(data, numberOfMeasurements)
    scaling_factor = zeros(1, numberOfMeasurements);
    scaling_factor_index = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Scaling_factor:')
            % Convert the third element to double and store it in temperatureData
            scaling_factor(1,scaling_factor_index) = ...
                str2double(temp_data{1}{2});
            scaling_factor_index = scaling_factor_index + 1;
        end
    end

    % Trim the array to the desired number of measurements
    scaling_factor = scaling_factor(1:numberOfMeasurements);
end
%  =======================================================================
function lifetime_GN_Data = get_lifetime_GN(data, numberOfMeasurements)
    lifetime_GN_Data = zeros(1, numberOfMeasurements);
    lifetime_GN_DataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Lifetime_GN:')
            % Convert the third element to double and store it in temperatureData
            lifetime_GN_Data(1,lifetime_GN_DataIndex) = ...
                str2double(temp_data{1}{2});
            lifetime_GN_DataIndex = lifetime_GN_DataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    lifetime_GN_Data = lifetime_GN_Data(1:numberOfMeasurements);
end


%  =======================================================================
function calibrated_lifetime_GN_Data = get_calibrated_lifetime_GN(data, numberOfMeasurements)
    calibrated_lifetime_GN_Data = zeros(1, numberOfMeasurements);
    calibrated_lifetime_GN_DataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Calibrated_Lifetime_GN:')
            % Convert the third element to double and store it in temperatureData
            calibrated_lifetime_GN_Data(1,calibrated_lifetime_GN_DataIndex) = ...
                str2double(temp_data{1}{2});
            calibrated_lifetime_GN_DataIndex = calibrated_lifetime_GN_DataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    calibrated_lifetime_GN_Data = calibrated_lifetime_GN_Data(1:numberOfMeasurements);
end

%  =======================================================================
function lifetime_BFGS_Data = get_lifetime_BFGS(data, numberOfMeasurements)
    lifetime_BFGS_Data = zeros(1, numberOfMeasurements);
    lifetime_BFGS_DataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Lifetime_BFGS:')
            % Convert the third element to double and store it in temperatureData
            lifetime_BFGS_Data(1,lifetime_BFGS_DataIndex) = ...
                str2double(temp_data{1}{2});
            lifetime_BFGS_DataIndex = lifetime_BFGS_DataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    lifetime_BFGS_Data = lifetime_BFGS_Data(1:numberOfMeasurements);
end


%  =======================================================================
function calibrated_lifetime_BFGS_Data = get_calibrated_lifetime_BFGS_Data(data, numberOfMeasurements)
    calibrated_lifetime_BFGS_Data = zeros(1, numberOfMeasurements);
    calibrated_lifetime_BFGS_DataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Calibrated_Lifetime_BFGS:')
            % Convert the third element to double and store it in temperatureData
            calibrated_lifetime_BFGS_Data(1,calibrated_lifetime_BFGS_DataIndex) = ...
                str2double(temp_data{1}{2});
            calibrated_lifetime_BFGS_DataIndex = calibrated_lifetime_BFGS_DataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    calibrated_lifetime_BFGS_Data = calibrated_lifetime_BFGS_Data(1:numberOfMeasurements);
end

%  =======================================================================
function lifetime_GD_Data = get_lifetime_GD(data, numberOfMeasurements)
    lifetime_GD_Data = zeros(1, numberOfMeasurements);
    lifetime_GD_DataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Lifetime_GD:')
            % Convert the third element to double and store it in temperatureData
            lifetime_GD_Data(1,lifetime_GD_DataIndex) = ...
                str2double(temp_data{1}{2});
            lifetime_GD_DataIndex = lifetime_GD_DataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    lifetime_GD_Data = lifetime_GD_Data(1:numberOfMeasurements);
end


%  =======================================================================
function calibrated_lifetime_GD_Data = get_calibrated_lifetime_GD(data, numberOfMeasurements)
    calibrated_lifetime_GD_Data = zeros(1, numberOfMeasurements);
    calibrated_lifetime_GD_DataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Calibrated_Lifetime_GD:')
            % Convert the third element to double and store it in temperatureData
            calibrated_lifetime_GD_Data(1,calibrated_lifetime_GD_DataIndex) = ...
                str2double(temp_data{1}{2});
            calibrated_lifetime_GD_DataIndex = calibrated_lifetime_GD_DataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    calibrated_lifetime_GD_Data = calibrated_lifetime_GD_Data(1:numberOfMeasurements);
end



% %  =======================================================================
% function lifetime_BFGS_Data = get_lifetime_BFGS(data, numberOfMeasurements)
%     lifetime_BFGS_Data = zeros(1, numberOfMeasurements);
%     lifetime_BFGS_DataIndex = 1;
% 
%     for i = 1:size(data, 1)
%         % Extracting the temperature data using textscan
%         temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
% 
%         % Check if the line contains temperature data
%         if strcmp(temp_data{1}{1}, 'Lifetime_BFGS:')
%             % Convert the third element to double and store it in temperatureData
%             lifetime_BFGS_Data(1,lifetime_BFGS_DataIndex) = ...
%                 str2double(temp_data{1}{2});
%             lifetime_BFGS_DataIndex = lifetime_BFGS_DataIndex + 1;
%         end
%     end
% 
%     % Trim the array to the desired number of measurements
%     lifetime_BFGS_Data = lifetime_BFGS_Data(1:numberOfMeasurements);
% end
% 
% 
% %  =======================================================================
% function calibrated_lifetime_BFGS_Data = calibrated_lifetime_BFGS_Data(data, numberOfMeasurements)
%     calibrated_lifetime_BFGS_Data = zeros(1, numberOfMeasurements);
%     calibrated_lifetime_BFGS_DataIndex = 1;
% 
%     for i = 1:size(data, 1)
%         % Extracting the temperature data using textscan
%         temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
% 
%         % Check if the line contains temperature data
%         if strcmp(temp_data{1}{1}, 'Calibrated_Lifetime_BFGS:')
%             % Convert the third element to double and store it in temperatureData
%             calibrated_lifetime_BFGS_Data(1,calibrated_lifetime_BFGS_DataIndex) = ...
%                 str2double(temp_data{1}{2});
%             calibrated_lifetime_BFGS_DataIndex = calibrated_lifetime_BFGS_DataIndex + 1;
%         end
%     end
% 
%     % Trim the array to the desired number of measurements
%     calibrated_lifetime_BFGS_Data = calibrated_lifetime_BFGS_Data(1:numberOfMeasurements);
% end


%  =======================================================================
function lifetime_GDM_Data = get_lifetime_GDM(data, numberOfMeasurements)
    lifetime_GDM_Data = zeros(1, numberOfMeasurements);
    lifetime_GDM_DataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Lifetime_GDM:')
            % Convert the third element to double and store it in temperatureData
            lifetime_GDM_Data(1,lifetime_GDM_DataIndex) = ...
                str2double(temp_data{1}{2});
            lifetime_GDM_DataIndex = lifetime_GDM_DataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    lifetime_GDM_Data = lifetime_GDM_Data(1:numberOfMeasurements);
end


%  =======================================================================
function calibrated_lifetime_GDM_Data = get_calibrated_lifetime_GDM(data, numberOfMeasurements)
    calibrated_lifetime_GDM_Data = zeros(1, numberOfMeasurements);
    calibrated_lifetime_GDM_DataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Calibrated_Lifetime_GDM:')
            % Convert the third element to double and store it in temperatureData
            calibrated_lifetime_GDM_Data(1,calibrated_lifetime_GDM_DataIndex) = ...
                str2double(temp_data{1}{2});
            calibrated_lifetime_GDM_DataIndex = calibrated_lifetime_GDM_DataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    calibrated_lifetime_GDM_Data = calibrated_lifetime_GDM_Data(1:numberOfMeasurements);
end

%  =======================================================================
function lifetime_GDA_Data = get_lifetime_GDA(data, numberOfMeasurements)
    lifetime_GDA_Data = zeros(1, numberOfMeasurements);
    lifetime_GDA_DataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Lifetime_GDA:')
            % Convert the third element to double and store it in temperatureData
            lifetime_GDA_Data(1,lifetime_GDA_DataIndex) = ...
                str2double(temp_data{1}{2});
            lifetime_GDA_DataIndex = lifetime_GDA_DataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    lifetime_GDA_Data = lifetime_GDA_Data(1:numberOfMeasurements);
end


%  =======================================================================
function calibrated_lifetime_GDA_Data = get_calibrated_lifetime_GDA(data, numberOfMeasurements)
    calibrated_lifetime_GDA_Data = zeros(1, numberOfMeasurements);
    calibrated_lifetime_GDA_DataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Calibrated_Lifetime_GDA:')
            % Convert the third element to double and store it in temperatureData
            calibrated_lifetime_GDA_Data(1,calibrated_lifetime_GDA_DataIndex) = ...
                str2double(temp_data{1}{2});
            calibrated_lifetime_GDA_DataIndex = calibrated_lifetime_GDA_DataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    calibrated_lifetime_GDA_Data = calibrated_lifetime_GDA_Data(1:numberOfMeasurements);
end

%  =======================================================================
function lifetime_LM_Data = get_lifetime_LM(data, numberOfMeasurements)
    lifetime_LM_Data = zeros(1, numberOfMeasurements);
    lifetime_LM_DataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Lifetime_LM:')
            % Convert the third element to double and store it in temperatureData
            lifetime_LM_Data(1,lifetime_LM_DataIndex) = ...
                str2double(temp_data{1}{2});
            lifetime_LM_DataIndex = lifetime_LM_DataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    lifetime_LM_Data = lifetime_LM_Data(1:numberOfMeasurements);
end


%  =======================================================================
function calibrated_lifetime_LM_Data = get_calibrated_lifetime_LM(data, numberOfMeasurements)
    calibrated_lifetime_LM_Data = zeros(1, numberOfMeasurements);
    calibrated_lifetime_LM_DataIndex = 1;

    for i = 1:size(data, 1)
        % Extracting the temperature data using textscan
        temp_data = textscan(data{i}, '%s', 'Delimiter', ' ');
        
        % Check if the line contains temperature data
        if strcmp(temp_data{1}{1}, 'Calibrated_Lifetime_LM:')
            % Convert the third element to double and store it in temperatureData
            calibrated_lifetime_LM_Data(1,calibrated_lifetime_LM_DataIndex) = ...
                str2double(temp_data{1}{2});
            calibrated_lifetime_LM_DataIndex = calibrated_lifetime_LM_DataIndex + 1;
        end
    end

    % Trim the array to the desired number of measurements
    calibrated_lifetime_LM_Data = calibrated_lifetime_LM_Data(1:numberOfMeasurements);
end