/*  Ekos Scheduler Greedy Algorithm
    SPDX-FileCopyrightText: Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QList>
#include <QMap>
#include <QDateTime>
#include <QString>
#include <QVector>
#include "schedulerjob.h"

namespace Ekos
{

class Scheduler;

class GreedyScheduler : public QObject
{
        Q_OBJECT

    public:

        // Result of a scheduling operation. Mostly useful for testing
        // or logging, as the true schedule is stored in the Qlist<SchedulerJob*>
        // returned by scheduleJobs().
        struct JobSchedule
        {
            SchedulerJob *job;
            QDateTime startTime;
            QDateTime stopTime;
            QString stopReason;
            JobSchedule(SchedulerJob *j, const QDateTime &start, const QDateTime &stop, const QString &r = "")
                : job(j), startTime(start), stopTime(stop), stopReason(r) {}
        };

        GreedyScheduler();
        /**
          * @brief setParams Sets parameters, usually stored as KStars Options to the scheduler.
          * @param restartImmediately Aborted jobs should attempt to be restarted right after they were suspended.
          * @param restartQueue Aborted jobs should attempt to be restarted after a delay, given below.
          * @param rescheduleErrors Jobs that failed because of errors should be restarted after a delay.
          * @param abortDelay The minimum delay (seconds) for restarting aborted jobs.
          * @param errorHandlingDelay The minimum delay (seconds) for restarting jobs that failed with errors.
          */
        void setParams(bool restartImmediately, bool restartQueue,
                       bool rescheduleErrors, int abortDelay,
                       int errorHandlingDelay);
        /**
          * @brief scheduleJobs Computes the schedule for job to be run.
          * @param jobs A list of SchedulerJobs
          * @param now The time at which the scheduling should start.
          * @param capturedFramesCount A structure, computed by the scheduler, which keeps track of previous job progress.
          * @param scheduler A pointer to the scheduler object, useful for notifying the user. Can be nullptr.
          * @return returns a possibly sorted list of the same jobs input, but with state and start/end time changes.
          */
        QList<SchedulerJob *> scheduleJobs(QList<SchedulerJob *> &jobs,
                                           const QDateTime &now,
                                           const QMap<QString, uint16_t> &capturedFramesCount,
                                           Scheduler *scheduler);
        /**
          * @brief checkJob Checks to see if a job should continue running.
          * @param jobs A list of SchedulerJobs
          * @param now The time at which the decision should be made.
          * @param currentJob The currently running job, which may be continued or aborted.
          * @return returns true if the job should continue to run.
          */
        bool checkJob(QList<SchedulerJob *> &jobs,
                      const QDateTime &now,
                      SchedulerJob *currentJob);
        /**
          * @brief getScheduledJob Returns the first job scheduled. Must be called after scheduleJobs().
          * @return returns the first job scheduled by scheduleJobs(), or nullptr.
          */
        SchedulerJob *getScheduledJob()
        {
            return scheduledJob;
        }
        /**
          * @brief getSchedule Returns the QList<JobSchedule> computed by scheduleJobs().
          * @return returns the previously computed schedule.
          */
        const QList<JobSchedule> &getSchedule()
        {
            return schedule;
        }
        /**
          * @brief setRescheduleAbortsImmediate sets the rescheduleAbortsImmediate parameter.
          */
        void setRescheduleAbortsImmediate(bool value)
        {
            rescheduleAbortsImmediate = value;
        }
        /**
          * @brief setRescheduleAbortsQueue sets the rescheduleAbortsQueue parameter.
          */
        void setRescheduleAbortsQueue(bool value)
        {
            rescheduleAbortsQueue = value;
        }
        /**
          * @brief setRescheduleErrors sets the rescheduleErrors parameter.
          */
        void setRescheduleErrors(bool value)
        {
            rescheduleErrors = value;
        }
        /**
          * @brief  setAbortDelaySeconds sets the abortDelaySeconds parameter.
          */
        void setAbortDelaySeconds(int value)
        {
            abortDelaySeconds = value;
        }
        /**
          * @brief  setErrorDelaySeconds sets the errorDelaySeconds parameter.
          */
        void setErrorDelaySeconds(int value)
        {
            errorDelaySeconds = value;
        }

        // For debugging
        static void printJobs(const QList<SchedulerJob *> &jobs, const QDateTime &time, const QString &label = "");
        static void printSchedule(const QList<JobSchedule> &schedule);
        static QString jobScheduleString(const JobSchedule &jobSchedule);

    private:

        // Changes the states of the jons on the list, deciding which ones
        // can be scheduled by scheduleJobs().
        QList<SchedulerJob *> prepareJobsForEvaluation(
            QList<SchedulerJob *> &jobs, const QDateTime &now,
            const QMap<QString, uint16_t> &capturedFramesCount, Scheduler *scheduler, bool reestimateJobTime = true);

        // Removes the EVALUATION state, after eval is done.
        void unsetEvaluation(const QList<SchedulerJob *> &jobs);

        // If currentJob is nullptr, this is used to find the next job
        // to schedule. It returns a pointer to a job in jobs, or nullptr.
        // If currentJob is a pointer to a job in jobs, then it will return
        // either currentJob if it shouldn't be interrupted, or a pointer
        // to a job that should interrupt it.
        SchedulerJob *selectNextJob(const QList<SchedulerJob *> &jobs,
                                    const QDateTime &now,
                                    SchedulerJob *currentJob = nullptr,
                                    bool fullSchedule = false,
                                    QDateTime *when = nullptr,
                                    QDateTime *nextInterruption = nullptr,
                                    QString *interruptReason = nullptr,
                                    const QMap<QString, uint16_t> *capturedFramesCount = nullptr);

        // Simulate the running of the scheduler from time to endTime.
        // Used to find which jobs will be run in the future.
        void simulate(const QList<SchedulerJob *> &jobs, const QDateTime &time,
                      const QDateTime &endTime = QDateTime(),
                      const QMap<QString, uint16_t> *capturedFramesCount = nullptr);

        // Error/Abort restart parameters.
        // Defaults don't matter much, will be set by UI.
        bool rescheduleAbortsImmediate { false };
        bool rescheduleAbortsQueue { true };
        bool rescheduleErrors {false};
        int abortDelaySeconds { 3600 };
        int errorDelaySeconds { 3600 };

        // These are values computed by scheduleJobs(), stored, and returned
        // by getScheduledJob() and getSchedule().
        SchedulerJob *scheduledJob { nullptr };
        QList<JobSchedule> schedule;
};

}  // namespace Ekos
