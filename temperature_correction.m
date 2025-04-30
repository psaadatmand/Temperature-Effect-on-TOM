% MATLAB script to extract measurement data and apply temperature scaling factors
clc; clear; close all;

% Define the filename (assumed to be in the same directory)
filename = 'putty.log';

% Open the file for reading
fid = fopen(filename, 'r');

if fid == -1
    errorCorrected('File not found or unable to open.');
end

% Initialize arrays to store all extracted data
timestamps = [];
lifetime_cma = [];
temperature_cma = [];
plot_tau_histogram = true;

% Read file line by line
while ~feof(fid)
    line = fgetl(fid);

    % Extract relevant data
    if contains(line, '- Measurement timestamp (s):')
        timestamps = [timestamps; sscanf(line, '- Measurement timestamp (s): %f')];
    elseif contains(line, '- Lifetime (CMA) (us):')
        lifetime_cma = [lifetime_cma; sscanf(line, '- Lifetime (CMA) (us): %f')];
    elseif contains(line, '- Temperature (CMA) (C):')
        temperature_cma = [temperature_cma; sscanf(line, '- Temperature (CMA) (C): %f')];
    end
end

% Close file
fclose(fid);

% Check if values were found
if isempty(timestamps)
    errorCorrected('No measurement data found in the file.');
end

% Define the 2 degree lookup table
temp_min = 22:2:42;
temp_max = temp_min + 2;
scaling_factors = linspace(1, 1.14, length(temp_min));

% Uncomment to Define the 0.1 degree lookup table
% temp_min = 22:0.1:42;
% temp_max = temp_min + 2;
% scaling_factors = linspace(1, 1.14, length(temp_min));

% Apply scaling factors based on temperature
corrected_lifetime = lifetime_cma; % Initialize adjusted array

for i = 1:length(temperature_cma)
    temp = temperature_cma(i);
    %comment to use linear scaling factors
    % SF = 1 + 0.01*(temp-22); 
    % adjusted_lifetime_cma(i) = lifetime_cma(i) * SF;

   % % Find corresponding scaling factor
    for j = 1:length(temp_min)
        if temp >= temp_min(j) && temp < temp_max(j)
            corrected_lifetime(i) = lifetime_cma(i) * scaling_factors(j);
            break;
        end
    end
end


% Plot original vs adjusted Lifetime (CMA)
figure;
plot(temperature_cma, lifetime_cma, 'r--o', 'LineWidth', 2, 'DisplayName', 'Uncorrected \tau');
hold on;
plot(temperature_cma, corrected_lifetime, 'g--o', 'LineWidth', 2, 'DisplayName', 'Corrected \tau');
xlabel('Temperature (\circC)');
ylabel('\tau (\mus)');
legend();
grid on;
hold off;


%Calculate and plot Error percentage before and after LUT calibration
errormain = 100.*(lifetime_cma(1) - lifetime_cma)./lifetime_cma(1);
errorCorrected = 100.*(corrected_lifetime(1) - corrected_lifetime)./corrected_lifetime(1);

figure;
plot(temperature_cma, errorCorrected, 'g--o', 'LineWidth', 2, 'DisplayName', 'Corrected Error');
hold on;
plot(temperature_cma, errormain, 'r--o', 'LineWidth', 2, 'DisplayName', 'Uncorrected Error');
xlabel('Temperature (\circC)');
ylabel('Error (%)');
legend show;
grid on;

%Plot Scaling factor
figure;
plot(temp_min, scaling_factors, 'b--o', 'LineWidth', 2, 'DisplayName', 'SF');
xlabel('Temperature (\circC)');
ylabel('SF ');
% legend show;
grid on;


% Correction Iteration
new_SF = scaling_factors;
corrected_lifetime_2 = corrected_lifetime;

for i = 1:length(temperature_cma)
    temp = temperature_cma(i);
    %comment to use linear scaling factors
    % SF = 1 + 0.01*(temp-22); 
    % adjusted_lifetime_cma(i) = lifetime_cma(i) * SF;

   % % Find corresponding scaling factor
    for j = 1:length(temp_min)
        if temp >= temp_min(j) && temp < temp_max(j)
            new_SF(j) = scaling_factors(j) * (1 + (errorCorrected(i)/100));
            corrected_lifetime_2(i) = lifetime_cma(i) * new_SF(j);
            break;
        end
    end
end

% Plot original vs corrected Lifetime (CMA) via iteration
figure;
plot(temperature_cma, lifetime_cma, 'r--o', 'LineWidth', 2, 'DisplayName', 'Uncorrected \tau');
hold on;
plot(temperature_cma, corrected_lifetime_2, 'g--o', 'LineWidth', 2, 'DisplayName', 'Corrected \tau');
xlabel('Temperature (\circC)');
ylabel('\tau (\mus)');
legend();
grid on;
hold off;


figure;
plot(temp_min, new_SF, 'b--o', 'LineWidth', 2, 'DisplayName', 'SF');
xlabel('Temperature (\circC)');
ylabel('new SF ');
% legend show;
grid on;


% plot histogram
% change corrected_lifetime_2 to corrected_lifetime to compute the first
% method's histogram
figure;
if plot_tau_histogram == true
     if i > 3
        avrg = mean(corrected_lifetime_2(2:i-1));
        sigma = std(corrected_lifetime_2(2:i-1));
        histfit(corrected_lifetime_2(2:i-1),20);
        set(gca,'FontSize',15);
        xlabel("\tau [\mus]",'FontSize',15);
        ylabel('Count','FontSize',15);
        title(['\mu = ', num2str(avrg), ' \mus, \sigma = ', ...
               num2str(sigma), ' \mus']);
        xlim([avrg - 3 * sigma avrg + 3 * sigma]);
        drawnow
     end
end 

% Display confirmation message
fprintf('Applied temperature-based scaling factors to Lifetime (CMA) values.\n');
fprintf('Plot generated for comparison.\n');